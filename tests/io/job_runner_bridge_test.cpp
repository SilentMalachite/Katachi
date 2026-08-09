// JobRunnerBridge のテスト（Phase 2 T6）。
//
// ここだけは Qt のイベントループとファイルシステムを使う。バッチ実行の本番経路そのもの。
// tests/main.cpp が QCoreApplication を構築しているため、QTimer と QFutureWatcher が動く。
//
// テスト名は ASCII に限る（Phase 0 の知見）。
#include "core/CapabilityTable.hpp"
#include "core/ConversionSpec.hpp"
#include "core/ConvertError.hpp"
#include "core/FormatId.hpp"
#include "core/NamingRule.hpp"
#include "io/CollisionPolicy.hpp"
#include "io/JobRunner.hpp"
#include "io/JobRunnerBridge.hpp"

#include <QByteArray>
#include <QColor>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QIODevice>
#include <QImage>
#include <QImageWriter>
#include <QList>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

#include <catch2/catch_test_macros.hpp>

#include <variant>

namespace {

using katachi::core::CapabilityTable;
using katachi::core::ConversionSpec;
using katachi::core::ConvertError;
using katachi::core::formatIdFromString;
using katachi::core::NamePattern;
using katachi::io::BatchRequest;
using katachi::io::CollisionPolicy;
using katachi::io::JobOutcome;
using katachi::io::JobRunnerBridge;
using katachi::io::JobStatus;
using katachi::io::OutputDirectory;

constexpr auto allEntries = QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden;
constexpr int waitLimitMs = 60000;

const CapabilityTable& qtTable() {
    static const CapabilityTable table = CapabilityTable::buildFromQt();
    return table;
}

QString writePng(const QDir& dir, const QString& name, int edge) {
    QImage image(edge, edge, QImage::Format_RGB32);
    image.fill(QColor(10, 20, 30));

    const QString path = dir.filePath(name);
    QImageWriter writer(path, "png");
    REQUIRE(writer.write(image));
    return path;
}

QString writeBroken(const QDir& dir, const QString& name) {
    const QString path = dir.filePath(name);
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    file.write(QByteArray(64, '\x01'));
    file.close();
    return path;
}

QStringList makeInputs(const QDir& dir, int count, int edge) {
    QStringList paths;
    for (int index = 0; index < count; ++index) {
        paths.append(writePng(dir, QStringLiteral("input%1.png").arg(index), edge));
    }
    return paths;
}

BatchRequest requestFor(const QStringList& paths, const QString& outputDir,
                        CollisionPolicy collision, const QString& pattern) {
    BatchRequest request;
    request.sourcePaths = paths;
    request.outputDirectory = OutputDirectory{outputDir};
    request.pattern = NamePattern{pattern};
    request.spec.target = formatIdFromString(QStringLiteral("png"));
    request.collision = collision;
    return request;
}

// finished が届くまでイベントループを回す。届かない場合は false を返す。
bool waitForFinished(JobRunnerBridge& bridge) {
    QEventLoop loop;
    bool finished = false;

    QObject::connect(&bridge, &JobRunnerBridge::finished, &loop, [&finished, &loop] {
        finished = true;
        loop.quit();
    });
    QTimer::singleShot(waitLimitMs, &loop, &QEventLoop::quit);
    loop.exec();

    return finished;
}

} // namespace

TEST_CASE("the bridge finishes a small batch", "[io][bridge]") {
    QTemporaryDir input;
    QTemporaryDir output;
    REQUIRE(input.isValid());
    REQUIRE(output.isValid());
    const QStringList paths = makeInputs(QDir(input.path()), 5, 32);

    JobRunnerBridge bridge(qtTable());
    QList<JobOutcome> collected;
    int finishedCount = 0;
    QObject::connect(&bridge, &JobRunnerBridge::resultsReady, &bridge,
                     [&collected](const QList<JobOutcome>& batch) { collected.append(batch); });
    QObject::connect(&bridge, &JobRunnerBridge::finished, &bridge,
                     [&finishedCount] { ++finishedCount; });

    bridge.start(
        requestFor(paths, output.path(), CollisionPolicy::Skip, QStringLiteral("{name}.{ext}")));

    REQUIRE(waitForFinished(bridge));
    REQUIRE(finishedCount == 1);
    REQUIRE(collected.size() == 5);
    for (const JobOutcome& outcome : collected) {
        REQUIRE(outcome.status == JobStatus::Succeeded);
        REQUIRE_FALSE(outcome.outputPath.isEmpty());
    }
    REQUIRE(QDir(output.path()).entryList(allEntries).size() == 5);
    REQUIRE_FALSE(bridge.isRunning());
}

TEST_CASE("the bridge throttles progress to 200ms", "[io][bridge]") {
    // docs/spec-core.md §7: 進捗表示は 200ms 以下の間隔で更新しない。
    QTemporaryDir input;
    QTemporaryDir output;
    REQUIRE(input.isValid());
    REQUIRE(output.isValid());
    const QStringList paths = makeInputs(QDir(input.path()), 200, 32);

    JobRunnerBridge bridge(qtTable());
    REQUIRE(bridge.progressIntervalMs() == 200);

    int progressSignals = 0;
    QObject::connect(&bridge, &JobRunnerBridge::progressChanged, &bridge,
                     [&progressSignals](int, int) { ++progressSignals; });

    bridge.start(
        requestFor(paths, output.path(), CollisionPolicy::Skip, QStringLiteral("{name}.{ext}")));
    REQUIRE(waitForFinished(bridge));

    // 200 件でもシグナルは件数分は飛ばない。ワーカーは atomic を増やすだけで、
    // 発火は main thread の 200ms タイマが行う。
    REQUIRE(progressSignals < 200);
    REQUIRE(QDir(output.path()).entryList(allEntries).size() == 200);
}

TEST_CASE("cancel stops the batch and the app returns to idle", "[io][bridge]") {
    QTemporaryDir input;
    QTemporaryDir output;
    REQUIRE(input.isValid());
    REQUIRE(output.isValid());
    const QStringList paths = makeInputs(QDir(input.path()), 300, 64);

    JobRunnerBridge bridge(qtTable());
    bridge.start(
        requestFor(paths, output.path(), CollisionPolicy::Skip, QStringLiteral("{name}.{ext}")));
    bridge.cancel();

    // キャンセル後も finished は届く（そうでないと UI が戻れない）。
    REQUIRE(waitForFinished(bridge));
    REQUIRE_FALSE(bridge.isRunning());
    REQUIRE(QDir(output.path()).entryList(allEntries).size() < paths.size());
}

TEST_CASE("the bridge can start a second batch after a cancel", "[io][bridge]") {
    QTemporaryDir input;
    QTemporaryDir cancelled;
    QTemporaryDir output;
    REQUIRE(input.isValid());
    REQUIRE(cancelled.isValid());
    REQUIRE(output.isValid());
    const QStringList many = makeInputs(QDir(input.path()), 300, 64);

    JobRunnerBridge bridge(qtTable());
    bridge.start(
        requestFor(many, cancelled.path(), CollisionPolicy::Skip, QStringLiteral("{name}.{ext}")));
    bridge.cancel();
    REQUIRE(waitForFinished(bridge));

    // 2 回目は最後まで走ること。キャンセルの状態が残っていない。
    const QStringList few = many.mid(0, 5);
    QList<JobOutcome> collected;
    QObject::connect(&bridge, &JobRunnerBridge::resultsReady, &bridge,
                     [&collected](const QList<JobOutcome>& batch) { collected.append(batch); });

    bridge.start(
        requestFor(few, output.path(), CollisionPolicy::Skip, QStringLiteral("{name}.{ext}")));
    REQUIRE(waitForFinished(bridge));

    REQUIRE(collected.size() == 5);
    for (const JobOutcome& outcome : collected) {
        REQUIRE(outcome.status == JobStatus::Succeeded);
    }
    REQUIRE(QDir(output.path()).entryList(allEntries).size() == 5);
}

TEST_CASE("failed jobs stay in the results with a reason", "[io][bridge]") {
    QTemporaryDir input;
    QTemporaryDir output;
    REQUIRE(input.isValid());
    REQUIRE(output.isValid());
    QStringList paths = makeInputs(QDir(input.path()), 2, 32);
    paths.append(writeBroken(QDir(input.path()), QStringLiteral("broken.png")));

    JobRunnerBridge bridge(qtTable());
    QList<JobOutcome> collected;
    QObject::connect(&bridge, &JobRunnerBridge::resultsReady, &bridge,
                     [&collected](const QList<JobOutcome>& batch) { collected.append(batch); });

    bridge.start(
        requestFor(paths, output.path(), CollisionPolicy::Skip, QStringLiteral("{name}.{ext}")));
    REQUIRE(waitForFinished(bridge));

    REQUIRE(collected.size() == 3);

    int failed = 0;
    int succeeded = 0;
    for (const JobOutcome& outcome : collected) {
        if (outcome.status == JobStatus::Failed) {
            ++failed;
            REQUIRE(outcome.failure.has_value());
            REQUIRE(std::holds_alternative<ConvertError>(*outcome.failure));
            REQUIRE(std::get<ConvertError>(*outcome.failure) == ConvertError::DecodeFailed);
            REQUIRE(outcome.sourcePath.endsWith(QStringLiteral("broken.png")));
        } else if (outcome.status == JobStatus::Succeeded) {
            ++succeeded;
        }
    }

    // 失敗した 1 件は消えずに理由付きで残り、他は成功する。
    REQUIRE(failed == 1);
    REQUIRE(succeeded == 2);
}

TEST_CASE("the conversion runs off the main thread", "[io][bridge]") {
    // 受け入れ基準「ワーカースレッドから QWidget に触れていない」の実行時側。
    // 構造的な担保は katachi_io が Qt6::Widgets をリンクしないこと（+ INV7）。
    QTemporaryDir input;
    QTemporaryDir output;
    REQUIRE(input.isValid());
    REQUIRE(output.isValid());
    const QStringList paths = makeInputs(QDir(input.path()), 5, 32);

    JobRunnerBridge bridge(qtTable());
    QThread* signalThread = nullptr;
    QObject::connect(&bridge, &JobRunnerBridge::finished, &bridge,
                     [&signalThread] { signalThread = QThread::currentThread(); });

    bridge.start(
        requestFor(paths, output.path(), CollisionPolicy::Skip, QStringLiteral("{name}.{ext}")));
    REQUIRE(waitForFinished(bridge));

    // 変換はワーカースレッドで走った。
    REQUIRE(bridge.lastWorkerThread() != nullptr);
    REQUIRE(bridge.lastWorkerThread() != QThread::currentThread());
    // UI へ届くシグナルは main thread から出る。
    REQUIRE(signalThread == QThread::currentThread());
}

TEST_CASE("parallel renames never collide", "[io][bridge]") {
    // ADR-0009 の追補（案 A）。全件が同じ出力名を要求しても、
    // 予約を直列化しているので互いを上書きしない。
    QTemporaryDir input;
    QTemporaryDir output;
    REQUIRE(input.isValid());
    REQUIRE(output.isValid());
    const QStringList paths = makeInputs(QDir(input.path()), 50, 32);

    JobRunnerBridge bridge(qtTable());
    QList<JobOutcome> collected;
    QObject::connect(&bridge, &JobRunnerBridge::resultsReady, &bridge,
                     [&collected](const QList<JobOutcome>& batch) { collected.append(batch); });

    // 全件が out.png を要求する（{name} も {index} も使わない）。
    bridge.start(
        requestFor(paths, output.path(), CollisionPolicy::Rename, QStringLiteral("out.{ext}")));
    REQUIRE(waitForFinished(bridge));

    REQUIRE(collected.size() == 50);
    for (const JobOutcome& outcome : collected) {
        REQUIRE(outcome.status == JobStatus::Succeeded);
    }
    // 50 件が別々のファイルになっていること。1 つでも重なれば数が減る。
    REQUIRE(QDir(output.path()).entryList(allEntries).size() == 50);
}
