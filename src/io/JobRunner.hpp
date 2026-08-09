#pragma once

// 1 件の変換ジョブを実行する（docs/spec-core.md §6）。
//
// **ヘッダのみのテンプレート。`.cpp` を作らない**（docs/cpp-conventions.md §2.4）。
// Q_OBJECT を持つクラスはテンプレートにできないため、シグネチャの発行は
// 非テンプレートの JobRunnerBridge（T6）へ分離する。
//
// クラスの型引数は docs/spec-core.md §6 の表記どおり Sink と Progress の 2 つ。
// **入力側はメンバ関数テンプレートとして ByteSource で制約する。**
// こうすることで、テストは Qt のイベントループにもファイルシステムにも触れずに済む
// （docs/cpp-conventions.md §2.3 が JobRunner をテンプレートにした理由そのもの）。

#include "core/CapabilityTable.hpp"
#include "core/ConversionSpec.hpp"
#include "core/ConvertError.hpp"
#include "core/Converter.hpp"
#include "core/FormatId.hpp"
#include "core/NamingRule.hpp"
#include "core/Result.hpp"
#include "io/CollisionPolicy.hpp"
#include "io/IoConcepts.hpp"
#include "io/IoError.hpp"

#include <QByteArray>
#include <QString>

#include <atomic>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

namespace katachi::io {

// 基底型を明示するのは clang-tidy の performance-enum-size による（Phase 1 T2 の知見）。
enum class JobStatus : std::uint8_t {
    Succeeded,
    Skipped,  // 宛先が既にあり CollisionPolicy::Skip だった（ADR-0009）。失敗ではない
    Failed,   //
    Cancelled // 着手前にキャンセルされた（ADR-0010 の二段構え）
};

// 失敗の理由。3 つの層の列挙をそのまま持ち、独自の番号へ翻訳しない。
// 翻訳すると、どの層で何が起きたかが失われる。表示用の文言は app 層で作る。
using JobFailure = std::variant<core::ConvertError, core::NamingError, IoError>;

// 1 件の材料。**バイト列は持たない。** 1000 件のバッチで全件がメモリへ載らないように
// するため、入力は ByteSource から必要になった時点で読む（ADR-0008）。
struct JobItem {
    QString sourcePath;     // 記録用。読み出しは ByteSource が行う
    QString sourceBaseName; // 命名規則の {name}
    int index = 0;          // 命名規則の {index}
    core::NamePattern pattern;
    core::ConversionSpec spec;
};

// 1 件の結果。**バイト列を持たない。** QFuture は全件の結果を保持するため、
// ここへ出力バイト列を載せると 1000 件分がメモリに残る（ADR-0008）。
struct JobOutcome {
    QString sourcePath;
    QString outputPath; // 衝突解決後の実パス。**呼び出し側（T6 の Bridge）が入れる**
    JobStatus status = JobStatus::Failed; // 入れ忘れが成功に見えないよう Failed を既定にする
    std::optional<JobFailure> failure;
    std::vector<core::ConvertWarning> warnings;
};

// 進捗の共有カウンタ。**注入する。** グローバル可変状態を作らない（CLAUDE.md）。
// 並列実行では複数のワーカーが同じカウンタを増やすため atomic にする。
struct BatchCounter {
    std::atomic<int> completed = 0;
    int total = 0;
};

// 出力ファイル名を作る。純粋（ファイルシステムを見ない）。
//
// 拡張子は出力形式そのもの（FormatId）から作る。**文字列リテラルを書かない。**
// 別名のうちどれを使うかを選ぶ根拠が無いため、代表名をそのまま使う
// （jpeg なら photo.jpeg になる。ADR-0006 の代表名）。
//
// 衝突の解決はしない（ADR-0005）。それは FileSink と Bridge の仕事。
[[nodiscard]] inline core::Result<OutputFileName, core::NamingError>
outputFileNameFor(const JobItem& item) {
    using NameResult = core::Result<OutputFileName, core::NamingError>;

    const core::NameExtension extension{core::formatIdToString(item.spec.target)};
    const core::Result<QString, core::NamingError> name =
        core::resolveOutputName(item.sourceBaseName, item.index, item.pattern, extension);

    if (!name.isOk()) {
        return NameResult::err(name.error());
    }

    return NameResult::ok(OutputFileName{name.value()});
}

template <ByteSink Sink, ProgressSink Progress> class JobRunner {
public:
    JobRunner(const core::CapabilityTable& caps, Progress& progress, BatchCounter& counter) noexcept
        : caps_(&caps), progress_(&progress), counter_(&counter) {}

    // 1 件実行する。読み出し → 変換 → 書き出し → 進捗。
    //
    // **名前の決定と衝突の解決はここではしない。** Sink は呼び出し側が
    // 解決済みの名前で構築する（ADR-0009 の追補。予約は Bridge がロックの中で行う）。
    template <ByteSource Source>
    [[nodiscard]] JobOutcome runOne(Source& source, Sink& sink, const JobItem& item) {
        JobOutcome outcome;
        outcome.sourcePath = item.sourcePath;

        // ADR-0010 のキャンセル二段構え。QFuture::cancel() は待機中の項目を止めるが、
        // 実行中の項目を止めるとは公式ドキュメントに書かれていない。
        // **着手直前に自分で見て早期終了する。**
        if (progress_->isCancelled()) {
            outcome.status = JobStatus::Cancelled;
            // 処理していないため進捗には数えない。
            return outcome;
        }

        const core::Result<QByteArray, IoError> bytes = source.read();
        if (!bytes.isOk()) {
            return failed(outcome, JobFailure{bytes.error()});
        }

        const core::Result<core::ConversionOutput, core::ConvertError> converted =
            core::convert(bytes.value(), item.spec, *caps_);
        if (!converted.isOk()) {
            return failed(outcome, JobFailure{converted.error()});
        }

        outcome.warnings = converted.value().warnings;

        const core::Result<std::monostate, IoError> written = sink.write(converted.value().bytes);
        if (!written.isOk()) {
            // Skip による DestinationExists は失敗ではない（ADR-0009）。
            outcome.status = written.error() == IoError::DestinationExists ? JobStatus::Skipped
                                                                           : JobStatus::Failed;
            outcome.failure = JobFailure{written.error()};
            reportProgress();
            return outcome;
        }

        outcome.status = JobStatus::Succeeded;
        reportProgress();
        return outcome;
    }

private:
    [[nodiscard]] JobOutcome failed(JobOutcome& outcome, JobFailure failure) {
        outcome.status = JobStatus::Failed;
        outcome.failure = std::move(failure);
        reportProgress();
        return outcome;
    }

    void reportProgress() {
        // 失敗もスキップも「1 件終わった」ことに変わりはない。
        const int done = counter_->completed.fetch_add(1, std::memory_order_relaxed) + 1;
        progress_->onProgress(done, counter_->total);
    }

    // 参照メンバにしないのは clang-tidy の
    // cppcoreguidelines-avoid-const-or-ref-data-members による。
    // いずれも非所有。寿命は呼び出し側（T6 の Bridge）が保証する。
    const core::CapabilityTable* caps_;
    Progress* progress_;
    BatchCounter* counter_;
};

} // namespace katachi::io
