#pragma once

// バッチ実行の司令塔（docs/spec-core.md §6 / ADR-0010）。
//
// **非テンプレートの薄い QObject アダプタ。** Q_OBJECT を持つクラスはテンプレートに
// できない（moc がテンプレートを処理しない）ため、実行そのものは
// ヘッダのみのテンプレート JobRunner<Sink, Progress> が担う
// （docs/cpp-conventions.md §2.4）。
//
// このクラスが担うのは 4 つ。
//   1. QtConcurrent::mapped と QThreadPool による並列実行
//   2. ProgressSink の本番実装（ワーカーは atomic を増やすだけ。シグナルは出さない）
//   3. 200ms のタイマで進捗と結果をまとめて emit する（docs/spec-core.md §7）
//   4. 出力名の予約をロックの中で直列化する（ADR-0009 の追補）
//
// **Qt6::Widgets には触れない。** io はワーカー側の層（ADR-0010、不変条件 INV7）。

#include "core/CapabilityTable.hpp"
#include "core/ConversionSpec.hpp"
#include "core/NamingRule.hpp"
#include "io/CollisionPolicy.hpp"
#include "io/JobRunner.hpp"
#include "io/MemoryBudget.hpp"

#include <QFutureWatcher>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QThreadPool>
#include <QTimer>

#include <atomic>

namespace katachi::io {

// バッチの投入内容。**バイト列は持たない**（ADR-0008）。
struct BatchRequest {
    QStringList sourcePaths;
    OutputDirectory outputDirectory;
    core::NamePattern pattern;
    core::ConversionSpec spec;
    CollisionPolicy collision = defaultCollisionPolicy;
};

class JobRunnerBridge : public QObject {
    Q_OBJECT

public:
    explicit JobRunnerBridge(const core::CapabilityTable& caps, QObject* parent = nullptr);

    JobRunnerBridge(const JobRunnerBridge&) = delete;
    JobRunnerBridge& operator=(const JobRunnerBridge&) = delete;
    JobRunnerBridge(JobRunnerBridge&&) = delete;
    JobRunnerBridge& operator=(JobRunnerBridge&&) = delete;

    ~JobRunnerBridge() override;

    // --- ProgressSink の実装。**ワーカースレッドから呼ばれる。** ---
    //
    // シグナルは出さない。atomic を更新するだけ（docs/spec-core.md §7 の 200ms 制約を
    // 構造的に満たすため）。発火は main thread の QTimer が行う。
    void onProgress(int done, int total);

    [[nodiscard]] bool isCancelled() const noexcept;

    // --- main thread から使う ---

    void start(const BatchRequest& request);

    [[nodiscard]] bool isRunning() const;

    // 進捗タイマの間隔。docs/spec-core.md §7 の 200ms をテストで固定するために公開する。
    [[nodiscard]] int progressIntervalMs() const;

    // 最後に変換を実行したスレッド。**main thread と異なることを検証するために公開する**
    // （docs/phases.md §4 Phase 2「ワーカースレッドから QWidget に触れていない」）。
    [[nodiscard]] QThread* lastWorkerThread() const noexcept;

    // `public slots:` と書かないのは clang-tidy の readability-redundant-access-specifiers
    // による（slots は public へ展開されるため、直前の public: と重複する）。
    // 接続は必ず関数ポインタ形式で行う規約のため（docs/cpp-conventions.md §1）、
    // moc へスロットとして登録する必要がない。
    void cancel();

signals:
    void progressChanged(int done, int total);
    void resultsReady(const QList<katachi::io::JobOutcome>& outcomes);
    void finished();

private:
    // ワーカースレッドで 1 件実行する。
    [[nodiscard]] JobOutcome runJob(const JobItem& item, const BatchRequest& request);

    // 出力名を決めて予約する。**ロックの中は実在確認だけ**（ADR-0009 の追補）。
    [[nodiscard]] core::Result<QString, IoError> reservePath(const OutputFileName& fileName,
                                                             const BatchRequest& request);

    void countOne();
    void flush();
    void handleFinished();

    const core::CapabilityTable* caps_;

    QThreadPool pool_;
    QFutureWatcher<JobOutcome> watcher_;
    QTimer timer_;

    MemoryBudget budget_;
    BatchCounter counter_;

    // 出力名の予約（ADR-0009 の追補）。ロックの中で参照・更新する。
    QMutex mutex_;
    QSet<QString> reserved_;

    std::atomic<bool> cancelled_ = false;
    std::atomic<int> lastDone_ = 0;
    std::atomic<int> lastTotal_ = 0;
    std::atomic<QThread*> lastWorkerThread_ = nullptr;

    int emittedDone_ = -1;
    int emittedResults_ = 0;
};

} // namespace katachi::io
