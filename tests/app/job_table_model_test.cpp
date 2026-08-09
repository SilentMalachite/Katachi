// JobTableModel のテスト（Phase 2 T7）。
//
// 表示文言は app 層にしか無い（docs/adr/0010）。**期待値として文言そのものを固定する。**
// 文言を変えるときはテストも一緒に動くべきで、黙って変わってよいものではない。
//
// テスト名は ASCII に限る（Phase 0 の知見）。
#include "app/JobTableModel.hpp"
#include "core/ConvertError.hpp"
#include "io/IoError.hpp"
#include "io/JobRunner.hpp"

#include <QList>
#include <QModelIndex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <Qt>

#include <catch2/catch_test_macros.hpp>

namespace {

using katachi::app::JobTableModel;
using katachi::core::ConvertError;
using katachi::core::ConvertWarning;
using katachi::io::IoError;
using katachi::io::JobFailure;
using katachi::io::JobOutcome;
using katachi::io::JobStatus;

constexpr int sourceColumn = 0;
constexpr int outputColumn = 1;
constexpr int statusColumn = 2;
constexpr int reasonColumn = 3;

QStringList threePaths() {
    return {QStringLiteral("/tmp/in/a.png"), QStringLiteral("/tmp/in/b.png"),
            QStringLiteral("/tmp/in/c.png")};
}

JobOutcome outcomeFor(const QString& sourcePath, JobStatus status) {
    JobOutcome outcome;
    outcome.sourcePath = sourcePath;
    outcome.status = status;
    return outcome;
}

QString textAt(const JobTableModel& model, int row, int column) {
    return model.data(model.index(row, column), Qt::DisplayRole).toString();
}

} // namespace

TEST_CASE("the model exposes one row per job", "[app][model]") {
    JobTableModel model;
    model.setJobs(threePaths());

    REQUIRE(model.rowCount() == 3);
    REQUIRE(model.columnCount() == 4);

    // 入力列はファイル名を出す。パス全体は横に広がりすぎる。
    REQUIRE(textAt(model, 0, sourceColumn) == QStringLiteral("a.png"));
    REQUIRE(textAt(model, 2, sourceColumn) == QStringLiteral("c.png"));

    // 実行前は待機中。出力も理由も空。
    REQUIRE(textAt(model, 0, statusColumn) == QStringLiteral("待機中"));
    REQUIRE(textAt(model, 0, outputColumn).isEmpty());
    REQUIRE(textAt(model, 0, reasonColumn).isEmpty());

    for (int column = 0; column < model.columnCount(); ++column) {
        REQUIRE_FALSE(
            model.headerData(column, Qt::Horizontal, Qt::DisplayRole).toString().isEmpty());
    }
}

TEST_CASE("a failed job keeps its row and shows the reason", "[app][model]") {
    JobTableModel model;
    model.setJobs(threePaths());

    JobOutcome failed = outcomeFor(QStringLiteral("/tmp/in/b.png"), JobStatus::Failed);
    failed.failure = JobFailure{ConvertError::DecodeFailed};
    model.applyOutcomes({failed});

    // 行は消えない。
    REQUIRE(model.rowCount() == 3);
    REQUIRE(textAt(model, 1, statusColumn) == QStringLiteral("失敗"));
    REQUIRE_FALSE(textAt(model, 1, reasonColumn).isEmpty());
    // 他の行は巻き込まれない。
    REQUIRE(textAt(model, 0, statusColumn) == QStringLiteral("待機中"));
}

TEST_CASE("a skipped job is shown as skipped not failed", "[app][model]") {
    // ADR-0009: DestinationExists は失敗ではない。「スキップ（既存）」と表示する。
    JobTableModel model;
    model.setJobs(threePaths());

    JobOutcome skipped = outcomeFor(QStringLiteral("/tmp/in/a.png"), JobStatus::Skipped);
    skipped.failure = JobFailure{IoError::DestinationExists};
    JobOutcome failed = outcomeFor(QStringLiteral("/tmp/in/b.png"), JobStatus::Failed);
    failed.failure = JobFailure{IoError::WriteFailed};
    model.applyOutcomes({skipped, failed});

    REQUIRE(textAt(model, 0, statusColumn) == QStringLiteral("スキップ"));
    REQUIRE(textAt(model, 1, statusColumn) == QStringLiteral("失敗"));
    REQUIRE(textAt(model, 0, statusColumn) != textAt(model, 1, statusColumn));

    REQUIRE(model.failedCount() == 1);
    REQUIRE(model.skippedCount() == 1);
}

TEST_CASE("a successful job shows its output and any warning", "[app][model]") {
    JobTableModel model;
    model.setJobs(threePaths());

    JobOutcome done = outcomeFor(QStringLiteral("/tmp/in/a.png"), JobStatus::Succeeded);
    done.outputPath = QStringLiteral("/tmp/out/a.png");
    done.warnings.push_back(ConvertWarning::AlphaFlattenedFallback);
    model.applyOutcomes({done});

    REQUIRE(textAt(model, 0, statusColumn) == QStringLiteral("成功"));
    REQUIRE(textAt(model, 0, outputColumn) == QStringLiteral("a.png"));
    // 警告は理由列に出す。成功しているが、指定どおりには処理していない（ADR-0004）。
    REQUIRE_FALSE(textAt(model, 0, reasonColumn).isEmpty());
}

TEST_CASE("updating one row emits dataChanged for that row only", "[app][model]") {
    JobTableModel model;
    model.setJobs(threePaths());

    int changes = 0;
    int firstRow = -1;
    int lastRow = -1;
    QObject::connect(&model, &JobTableModel::dataChanged, &model,
                     [&](const QModelIndex& topLeft, const QModelIndex& bottomRight) {
                         ++changes;
                         firstRow = topLeft.row();
                         lastRow = bottomRight.row();
                     });

    model.applyOutcomes({outcomeFor(QStringLiteral("/tmp/in/c.png"), JobStatus::Succeeded)});

    REQUIRE(changes == 1);
    REQUIRE(firstRow == 2);
    REQUIRE(lastRow == 2);
}

TEST_CASE("the model never reorders rows", "[app][model]") {
    // 並列実行では完了順が入力順と一致しない。**表の並びは追加順のまま。**
    // 勝手に動くと、利用者が見ている行が入れ替わる（docs/spec-core.md §7）。
    JobTableModel model;
    model.setJobs(threePaths());

    model.applyOutcomes({outcomeFor(QStringLiteral("/tmp/in/c.png"), JobStatus::Succeeded),
                         outcomeFor(QStringLiteral("/tmp/in/a.png"), JobStatus::Succeeded)});

    REQUIRE(textAt(model, 0, sourceColumn) == QStringLiteral("a.png"));
    REQUIRE(textAt(model, 1, sourceColumn) == QStringLiteral("b.png"));
    REQUIRE(textAt(model, 2, sourceColumn) == QStringLiteral("c.png"));
    REQUIRE(textAt(model, 1, statusColumn) == QStringLiteral("待機中"));
}

TEST_CASE("an unknown outcome is ignored", "[app][model]") {
    // 表に無い入力の結果が来ても落ちない。行も増えない。
    JobTableModel model;
    model.setJobs(threePaths());

    model.applyOutcomes({outcomeFor(QStringLiteral("/tmp/in/zzz.png"), JobStatus::Succeeded)});

    REQUIRE(model.rowCount() == 3);
    REQUIRE(textAt(model, 0, statusColumn) == QStringLiteral("待機中"));
}
