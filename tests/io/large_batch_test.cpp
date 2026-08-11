// 受け入れ基準の検証（Phase 2 T10）。
//
// docs/phases.md §4 Phase 2:
//   - 1000 ファイルのバッチで UI が固まらない
//   - （ADR-0008）同時確保量が予算を超えない
//
// **1000 件を実際に流す。** 少ない件数で代用すると、確かめたい性質が現れない。
//
// テスト名は ASCII に限る（Phase 0 の知見）。
#include "core/CapabilityTable.hpp"
#include "core/FormatId.hpp"
#include "core/NamingRule.hpp"
#include "io/CollisionPolicy.hpp"
#include "io/JobRunner.hpp"
#include "io/JobRunnerBridge.hpp"

#include <QColor>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QImage>
#include <QImageWriter>
#include <QList>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QTimer>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

namespace {

using katachi::core::CapabilityTable;
using katachi::core::formatIdFromString;
using katachi::core::NamePattern;
using katachi::io::BatchRequest;
using katachi::io::CollisionPolicy;
using katachi::io::JobOutcome;
using katachi::io::JobRunnerBridge;
using katachi::io::JobStatus;
using katachi::io::OutputDirectory;

constexpr int batchSize = 1000;
constexpr int edge = 8;
constexpr int waitLimitMs = 300000;
// main thread のイベントループを見張る間隔。
constexpr int pollIntervalMs = 10;
// 「固まっていない」と言える上限。CI のばらつきに耐える幅を取る。
constexpr qint64 maxAllowedGapMs = 500;

const CapabilityTable& qtTable() {
    static const CapabilityTable table = CapabilityTable::buildFromQt();
    return table;
}

} // namespace

TEST_CASE("a batch of 1000 files completes without blocking the main thread",
          "[io][bridge][slow]") {
    QTemporaryDir input;
    QTemporaryDir output;
    REQUIRE(input.isValid());
    REQUIRE(output.isValid());

    const QDir inputDir(input.path());
    QStringList paths;
    paths.reserve(batchSize);
    {
        QImage image(edge, edge, QImage::Format_RGB32);
        image.fill(QColor(7, 8, 9));
        for (int index = 0; index < batchSize; ++index) {
            const QString path = inputDir.filePath(QStringLiteral("input%1.png").arg(index));
            QImageWriter writer(path, "png");
            REQUIRE(writer.write(image));
            paths.append(path);
        }
    }
    REQUIRE(paths.size() == batchSize);

    JobRunnerBridge bridge(qtTable());

    QList<JobOutcome> collected;
    int progressSignals = 0;
    QObject::connect(&bridge, &JobRunnerBridge::resultsReady, &bridge,
                     [&collected](const QList<JobOutcome>& batch) { collected.append(batch); });
    QObject::connect(&bridge, &JobRunnerBridge::progressChanged, &bridge,
                     [&progressSignals](int, int) { ++progressSignals; });

    // main thread が生きているかを測る。変換がここで走っていれば間隔が空く。
    qint64 maxGapMs = 0;
    int ticks = 0;
    QElapsedTimer sinceLastTick;
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, &poll, [&] {
        ++ticks;
        maxGapMs = std::max(maxGapMs, sinceLastTick.elapsed());
        sinceLastTick.restart();
    });

    BatchRequest request;
    request.sourcePaths = paths;
    request.outputDirectory = OutputDirectory{output.path()};
    request.pattern = NamePattern{QStringLiteral("{name}.{ext}")};
    request.spec.target = formatIdFromString(QStringLiteral("png"));
    request.collision = CollisionPolicy::Skip;

    QEventLoop loop;
    bool finished = false;
    QObject::connect(&bridge, &JobRunnerBridge::finished, &loop, [&finished, &loop] {
        finished = true;
        loop.quit();
    });
    QTimer::singleShot(waitLimitMs, &loop, &QEventLoop::quit);

    // バッチの所要時間。検査 3 の上限をここから計算する。
    QElapsedTimer batchClock;

    batchClock.start();
    sinceLastTick.start();
    poll.start(pollIntervalMs);
    bridge.start(request);
    loop.exec();
    const qint64 batchMs = batchClock.elapsed();
    poll.stop();

    REQUIRE(finished);

    // 1. 全件が完了する。
    REQUIRE(collected.size() == batchSize);
    const auto succeeded = std::ranges::count_if(collected, [](const JobOutcome& outcome) {
        return outcome.status == JobStatus::Succeeded;
    });
    REQUIRE(succeeded == batchSize);
    REQUIRE(QDir(output.path()).entryList(QDir::Files).size() == batchSize);

    // 2. main thread のイベントループが回り続けた。
    //    変換が main thread で走っていれば、ここが数百 ms 単位で空く。
    //
    // **判定は「最大の間隔」で行う。** 発火回数はバッチの所要時間に比例するので、
    // 速い機械ほど小さくなる。回数に閾値を置くと「速いから落ちる」テストになる。
    REQUIRE(ticks >= 1);
    REQUIRE(maxGapMs < maxAllowedGapMs);

    // 3. 進捗シグナルは 200ms の間引きを守る（docs/spec-core.md §7）。
    //
    // **上限は経過時間から計算する。** 回数に固定の閾値を置くと、上の 2 と同じ理由で
    // 「遅い機械だから落ちる」テストになる。実際 Windows の CI で落ちた（Phase 4 T2）:
    // バッチが 24.7 秒かかり、間引きが正しく効いた結果の 103 回が `< 100` に触れた。
    // 24.7 秒 / 200ms = 123 回まではありうるので、仕様は満たされていた。
    //
    // 間隔は JobRunnerBridge が持つ値をそのまま読む。同じ数字を二重に書かない。
    const int intervalMs = bridge.progressIntervalMs();
    REQUIRE(intervalMs > 0);
    const auto maxSignals = static_cast<int>(batchMs / intervalMs) + 2; // 端数と開始・終了の分
    REQUIRE(progressSignals <= maxSignals);

    // 件数ごとには飛ばない。上の上限は経過時間に比例するため、極端に遅い機械では
    // 1 件 1 回の実装でも通りうる。**元の検査の意図をここに別立てで残す。**
    REQUIRE(progressSignals < batchSize);

    // 4. 同時確保量が予算を超えない（ADR-0008）。0 なら予算を通っていない＝検証になっていない。
    REQUIRE(bridge.peakBudgetUnits() > 0);
    REQUIRE(bridge.peakBudgetUnits() <= bridge.budgetTotalUnits());
}
