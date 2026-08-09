// UI の応答性の検証（Phase 2 T10）。
//
// tests/io/large_batch_test.cpp が Bridge の層で 1000 件を確かめるのに対し、
// **ここは MainWindow を通した実際の経路**を確かめる。件数を抑えるのは、
// 同じ規模を 2 度流すと CI の時間が二重にかかるため。
//
// テスト名は ASCII に限る（Phase 0 の知見）。
#include "app/MainWindow.hpp"
#include "core/CapabilityTable.hpp"

#include <QApplication>
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
#include <QUrl>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

namespace {

using katachi::app::MainWindow;
using katachi::core::CapabilityTable;

constexpr int batchSize = 200;
constexpr int edge = 8;
constexpr int waitLimitMs = 120000;
constexpr int pollIntervalMs = 10;
constexpr qint64 maxAllowedGapMs = 500;

const CapabilityTable& qtTable() {
    static const CapabilityTable table = CapabilityTable::buildFromQt();
    return table;
}

} // namespace

TEST_CASE("the window stays responsive during a batch", "[app][window][slow]") {
    QTemporaryDir input;
    QTemporaryDir output;
    REQUIRE(input.isValid());
    REQUIRE(output.isValid());

    const QDir inputDir(input.path());
    {
        QImage image(edge, edge, QImage::Format_RGB32);
        image.fill(QColor(4, 5, 6));
        for (int index = 0; index < batchSize; ++index) {
            QImageWriter writer(inputDir.filePath(QStringLiteral("input%1.png").arg(index)), "png");
            REQUIRE(writer.write(image));
        }
    }

    MainWindow window(qtTable());
    window.addSources({QUrl::fromLocalFile(input.path())});
    REQUIRE(window.jobCount() == batchSize);
    window.setOutputDirectory(output.path());

    qint64 maxGapMs = 0;
    int ticks = 0;
    QElapsedTimer sinceLastTick;

    QEventLoop loop;
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, &poll, [&] {
        ++ticks;
        maxGapMs = std::max(maxGapMs, sinceLastTick.elapsed());
        sinceLastTick.restart();
        if (!window.isConverting()) {
            loop.quit();
        }
    });
    QTimer::singleShot(waitLimitMs, &loop, &QEventLoop::quit);

    sinceLastTick.start();
    poll.start(pollIntervalMs);
    window.startConversion();
    REQUIRE(window.isConverting());
    loop.exec();
    poll.stop();

    REQUIRE_FALSE(window.isConverting());
    REQUIRE(window.succeededCount() == batchSize);
    REQUIRE(window.failedCount() == 0);
    // 完了しても行は消えない。
    REQUIRE(window.jobCount() == batchSize);
    REQUIRE_FALSE(window.statusText().isEmpty());

    // main thread が塞がっていない。塞がっていればここが数百 ms 単位で空く。
    //
    // **件数ではなく「最大の間隔」で判定する。** 発火回数はバッチの所要時間に比例するため、
    // 速い機械では数回しか発火せず、閾値を置くと「速いから落ちる」テストになる
    // （実測: 200 件が約 40ms で終わり 4 回しか発火しなかった）。
    // 負荷をかけた状態での確認は tests/io/large_batch_test.cpp（1000 件）が担う。
    REQUIRE(ticks >= 1);
    REQUIRE(maxGapMs < maxAllowedGapMs);
}
