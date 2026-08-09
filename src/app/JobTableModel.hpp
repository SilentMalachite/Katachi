#pragma once

// ジョブ一覧の表（docs/spec-core.md §1 / ADR-0010）。
//
// 列は 入力 / 出力 / 状態 / 理由 の 4 つ。
// **失敗した行を消さない**（docs/phases.md §4 Phase 2 の受け入れ基準）。
// **行を並べ替えない。** 並列実行では完了順が入力順と一致しないが、
// 表の並びが勝手に動くと利用者が見ている行が入れ替わる（docs/spec-core.md §7）。
//
// 表示用の文言はこの層にしか置かない（ADR-0010）。

#include "io/JobRunner.hpp"

#include <QAbstractTableModel>
#include <QHash>
#include <QList>
#include <QModelIndex>
#include <QObject>
#include <QString>
#include <QVariant>
#include <Qt>

#include <optional>

namespace katachi::app {

class JobTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit JobTableModel(QObject* parent = nullptr);

    // 実行前の一覧を作る。既存の行は捨てる。
    //
    // QStringList と綴らないのは clang-tidy の misc-include-cleaner による。
    // Qt 6 の QStringList は qcontainerfwd.h 由来の QList<QString> の別名で、
    // <QStringList> を include しても提供ヘッダと認められない（T1.5 と同じ事情）。
    void setJobs(const QList<QString>& sourcePaths);

    // 実行結果を反映する。**行は増えも減りもしない。**
    // 表に無い入力の結果は黙って捨てる（落ちない）。
    void applyOutcomes(const QList<io::JobOutcome>& outcomes);

    void clear();

    [[nodiscard]] int failedCount() const noexcept;
    [[nodiscard]] int skippedCount() const noexcept;
    [[nodiscard]] int succeededCount() const noexcept;

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                      int role) const override;

private:
    struct Row {
        QString sourcePath;
        std::optional<io::JobOutcome> outcome;
    };

    QList<Row> rows_;
    QHash<QString, int> rowBySourcePath_;
};

} // namespace katachi::app
