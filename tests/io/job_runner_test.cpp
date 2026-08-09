// JobRunner のテスト（Phase 2 T4）。
//
// **Qt のイベントループにもファイルシステムにも触れない。**
// 入力画像はその場でメモリ上に符号化して作る。
// これが docs/cpp-conventions.md §2.3 が JobRunner をテンプレートにした理由。
//
// テスト名は ASCII に限る（Phase 0 の知見）。
#include "core/CapabilityTable.hpp"
#include "core/ConversionSpec.hpp"
#include "core/ConvertError.hpp"
#include "core/Converter.hpp"
#include "core/FormatId.hpp"
#include "core/NamingRule.hpp"
#include "core/Result.hpp"
#include "io/CollisionPolicy.hpp"
#include "io/IoError.hpp"
#include "io/JobRunner.hpp"
#include "test_doubles.hpp"

#include <QBuffer>
#include <QByteArray>
#include <QColor>
#include <QIODevice>
#include <QImage>
#include <QImageWriter>
#include <QString>

#include <catch2/catch_test_macros.hpp>

#include <variant>

namespace {

using katachi::core::AlphaPolicy;
using katachi::core::CapabilityTable;
using katachi::core::ConversionSpec;
using katachi::core::ConvertError;
using katachi::core::ConvertWarning;
using katachi::core::formatIdFromString;
using katachi::core::NameExtension;
using katachi::core::NamePattern;
using katachi::core::NamingError;
using katachi::io::BatchCounter;
using katachi::io::IoError;
using katachi::io::JobItem;
using katachi::io::JobRunner;
using katachi::io::JobStatus;
using katachi::io::outputFileNameFor;
using katachi::test::FailingSink;
using katachi::test::FakeProgress;
using katachi::test::MemorySink;
using katachi::test::MemorySource;

constexpr int edge = 16;
constexpr int channelMax = 255;

const CapabilityTable& qtTable() {
    static const CapabilityTable table = CapabilityTable::buildFromQt();
    return table;
}

// メモリ上で PNG を作る。ファイルシステムに触れないため。
QByteArray encodedPng(bool withAlpha) {
    QImage image(edge, edge, withAlpha ? QImage::Format_ARGB32 : QImage::Format_RGB32);
    for (int row = 0; row < edge; ++row) {
        for (int column = 0; column < edge; ++column) {
            const int value = (column * channelMax) / (edge - 1);
            image.setPixelColor(column, row,
                                QColor(value, channelMax - value, row * (channelMax / edge),
                                       withAlpha ? value : channelMax));
        }
    }
    if (withAlpha) {
        image.setPixelColor(0, 0, QColor(0, 0, 0, 0));
    }

    QByteArray bytes;
    QBuffer buffer(&bytes);
    REQUIRE(buffer.open(QIODevice::WriteOnly));
    QImageWriter writer(&buffer, "png");
    REQUIRE(writer.write(image));
    return bytes;
}

ConversionSpec specFor(const QString& target) {
    ConversionSpec spec;
    spec.target = formatIdFromString(target);
    return spec;
}

JobItem itemFor(const QString& target) {
    JobItem item;
    item.sourcePath = QStringLiteral("/tmp/photo.png");
    item.sourceBaseName = QStringLiteral("photo");
    item.index = 1;
    item.pattern = NamePattern{QStringLiteral("{name}.{ext}")};
    item.spec = specFor(target);
    return item;
}

} // namespace

TEST_CASE("runOne converts and writes through the sink", "[io][jobrunner]") {
    const QByteArray source = encodedPng(false);
    MemorySource input(source);
    MemorySink sink;
    FakeProgress progress;
    BatchCounter counter{.completed = 0, .total = 1};

    JobRunner<MemorySink, FakeProgress> runner(qtTable(), progress, counter);
    const auto outcome = runner.runOne(input, sink, itemFor(QStringLiteral("png")));

    REQUIRE(outcome.status == JobStatus::Succeeded);
    REQUIRE_FALSE(outcome.failure.has_value());
    REQUIRE(outcome.sourcePath == QStringLiteral("/tmp/photo.png"));

    // core::convert() が返すバイト列そのものが書かれること。
    const auto expected = katachi::core::convert(source, specFor(QStringLiteral("png")), qtTable());
    REQUIRE(expected.isOk());
    REQUIRE(sink.written() == expected.value().bytes);
}

TEST_CASE("runOne reports progress once per item", "[io][jobrunner]") {
    const QByteArray source = encodedPng(false);
    FakeProgress progress;
    BatchCounter counter{.completed = 0, .total = 10};
    JobRunner<MemorySink, FakeProgress> runner(qtTable(), progress, counter);

    for (int job = 0; job < 10; ++job) {
        MemorySource input(source);
        MemorySink sink;
        REQUIRE(runner.runOne(input, sink, itemFor(QStringLiteral("png"))).status ==
                JobStatus::Succeeded);
    }

    REQUIRE(progress.calls().size() == 10);
    for (std::size_t call = 0; call < progress.calls().size(); ++call) {
        // done は 1 から順に増え、total は据え置き。
        REQUIRE(progress.calls().at(call).first == static_cast<int>(call) + 1);
        REQUIRE(progress.calls().at(call).second == 10);
    }
}

TEST_CASE("runOne skips work when already cancelled", "[io][jobrunner]") {
    // ADR-0010 のキャンセル二段構え。着手直前に見て早期終了する。
    MemorySource input(encodedPng(false));
    MemorySink sink;
    FakeProgress progress;
    progress.cancel();
    BatchCounter counter{.completed = 0, .total = 1};

    JobRunner<MemorySink, FakeProgress> runner(qtTable(), progress, counter);
    const auto outcome = runner.runOne(input, sink, itemFor(QStringLiteral("png")));

    REQUIRE(outcome.status == JobStatus::Cancelled);
    // Sink は呼ばれない。進捗も数えない（処理していないため）。
    REQUIRE(sink.written().isEmpty());
    REQUIRE(progress.calls().empty());
}

TEST_CASE("runOne records a decode failure with its reason", "[io][jobrunner]") {
    MemorySource input(QByteArray(64, '\x01'));
    MemorySink sink;
    FakeProgress progress;
    BatchCounter counter{.completed = 0, .total = 1};

    JobRunner<MemorySink, FakeProgress> runner(qtTable(), progress, counter);
    const auto outcome = runner.runOne(input, sink, itemFor(QStringLiteral("png")));

    REQUIRE(outcome.status == JobStatus::Failed);
    REQUIRE(outcome.failure.has_value());
    REQUIRE(std::holds_alternative<ConvertError>(*outcome.failure));
    REQUIRE(std::get<ConvertError>(*outcome.failure) == ConvertError::DecodeFailed);
    REQUIRE(sink.written().isEmpty());
    // 失敗した件も進捗としては 1 件進む。
    REQUIRE(progress.calls().size() == 1);
}

TEST_CASE("runOne records a sink failure with its reason", "[io][jobrunner]") {
    MemorySource input(encodedPng(false));
    FailingSink sink;
    FakeProgress progress;
    BatchCounter counter{.completed = 0, .total = 1};

    JobRunner<FailingSink, FakeProgress> runner(qtTable(), progress, counter);
    const auto outcome = runner.runOne(input, sink, itemFor(QStringLiteral("png")));

    REQUIRE(outcome.status == JobStatus::Failed);
    REQUIRE(outcome.failure.has_value());
    REQUIRE(std::holds_alternative<IoError>(*outcome.failure));
    REQUIRE(std::get<IoError>(*outcome.failure) == IoError::WriteFailed);
}

TEST_CASE("runOne carries the converter warnings", "[io][jobrunner]") {
    // docs/spec-core.md §4 の 2 行目。アルファ有りを非対応形式へ Preserve で送る。
    MemorySource input(encodedPng(true));
    MemorySink sink;
    FakeProgress progress;
    BatchCounter counter{.completed = 0, .total = 1};

    JobItem item = itemFor(QStringLiteral("jpeg"));
    item.spec.alpha = AlphaPolicy::Preserve;

    JobRunner<MemorySink, FakeProgress> runner(qtTable(), progress, counter);
    const auto outcome = runner.runOne(input, sink, item);

    REQUIRE(outcome.status == JobStatus::Succeeded);
    REQUIRE(outcome.warnings.size() == 1);
    REQUIRE(outcome.warnings.front() == ConvertWarning::AlphaFlattenedFallback);
}

TEST_CASE("outputFileNameFor builds the name from the pattern and the target format",
          "[io][jobrunner]") {
    JobItem item = itemFor(QStringLiteral("png"));
    item.pattern = NamePattern{QStringLiteral("{name}_{index:03}.{ext}")};

    const auto name = outputFileNameFor(item);

    REQUIRE(name.isOk());
    REQUIRE(name.value().v == QStringLiteral("photo_001.png"));
}

TEST_CASE("outputFileNameFor uses the chosen extension", "[io][jobrunner]") {
    // 利用者が別名を選べる（T8 で SettingsPanel から渡す）。
    // 指定が無ければ代表名（ADR-0006）へ落ちる。
    JobItem item = itemFor(QStringLiteral("jpeg"));
    item.pattern = NamePattern{QStringLiteral("{name}.{ext}")};

    const auto fallback = outputFileNameFor(item);
    REQUIRE(fallback.isOk());
    REQUIRE(fallback.value().v == QStringLiteral("photo.jpeg"));

    item.extension = NameExtension{QStringLiteral("jpg")};

    const auto chosen = outputFileNameFor(item);
    REQUIRE(chosen.isOk());
    REQUIRE(chosen.value().v == QStringLiteral("photo.jpg"));
}

TEST_CASE("outputFileNameFor reports a naming failure with its reason", "[io][jobrunner]") {
    JobItem item = itemFor(QStringLiteral("png"));
    item.pattern = NamePattern{QString()};

    const auto name = outputFileNameFor(item);

    REQUIRE_FALSE(name.isOk());
    REQUIRE(name.error() == NamingError::EmptyPattern);
}
