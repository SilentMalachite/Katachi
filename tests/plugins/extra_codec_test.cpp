// 「能力表が追加コーデックを自動的に反映する（コード変更不要）」の機械検査。
// docs/phases.md §4 Phase 3 の受け入れ基準 3。
//
// **src/ を 1 行も変えずに、プラグインを置くだけで能力表へ現れることを確かめる。**
// 実コーデック（libavif / libjxl / LibRaw）が無い環境でも成立するよう、
// tests/plugins/TestFormatPlugin.cpp が提供する架空の形式を使う。
//
// KATACHI_EXTRA_CODECS=ON でビルドした場合、QT_PLUGIN_PATH には実コーデックの
// 配置先も入る（tests/CMakeLists.txt）。そのとき下の「全 encodable 形式」を
// 走る検査は実コーデックも対象に含む。**テスト側に形式名を足す必要は無い。**
//
// テストコードはフォーマット名の文字列リテラル禁止の対象外（docs/spec-core.md §3）。
// テスト名は ASCII に限る（Windows のコンソール encoding で化けるため）。
#include "core/CapabilityTable.hpp"
#include "core/ConversionSpec.hpp"
#include "core/Converter.hpp"
#include "core/FormatId.hpp"

#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QImage>
#include <QImageWriter>
#include <QLatin1Char>
#include <QSet>
#include <QString>

#include <catch2/catch_test_macros.hpp>

#include <vector>

using katachi::core::CapabilityTable;
using katachi::core::ConversionSpec;
using katachi::core::convert;
using katachi::core::FormatCapability;
using katachi::core::FormatId;
using katachi::core::formatIdFromString;
using katachi::core::formatIdToString;

namespace {

// tests/plugins/testformat.json の Keys と一致させる。
FormatId testFormat() { return formatIdFromString(QStringLiteral("katachitest")); }

QByteArray fixture(const QString& name) {
    QFile file(QString::fromUtf8(KATACHI_FIXTURE_DIR) + QLatin1Char('/') + name);
    REQUIRE(file.open(QIODevice::ReadOnly));
    return file.readAll();
}

QSet<QString> normalizedWritableFormats() {
    QSet<QString> names;
    for (const QByteArray& name : QImageWriter::supportedImageFormats()) {
        names.insert(formatIdFromString(QString::fromUtf8(name)).v);
    }
    return names;
}

} // namespace

// ---------------------------------------------------------------- 前提の確認

TEST_CASE("the test plugin is visible to Qt", "[plugin][precondition]") {
    // ここが偽なら以下のテストは空振りする。QT_PLUGIN_PATH の設定漏れを検出する。
    REQUIRE(normalizedWritableFormats().contains(testFormat().v));
}

// ------------------------------------------------ 能力表への自動反映（基準 3）

TEST_CASE("a newly added plugin appears in the capability table", "[plugin][capability]") {
    const CapabilityTable table = CapabilityTable::buildFromQt();
    const auto capability = table.find(testFormat());

    REQUIRE(capability.has_value());
    REQUIRE(capability->canDecode);
    REQUIRE(capability->canEncode);
}

TEST_CASE("the probes classify the new plugin by measurement", "[plugin][capability]") {
    // ADR-0007 はアルファと可逆性をメモリ上の往復で実測する。
    // TestFormatPlugin は無圧縮の ARGB32 を書くため、両方 true になるはずである。
    const CapabilityTable table = CapabilityTable::buildFromQt();
    const auto capability = table.find(testFormat());

    REQUIRE(capability.has_value());
    REQUIRE(capability->supportsAlpha);
    REQUIRE(capability->isLossless);
}

TEST_CASE("the new plugin becomes an output choice", "[plugin][capability]") {
    // src/app/SettingsPanel.cpp は encodable() から出力形式の候補を作る。
    // ここに現れることが「UI に自動で載る」ことと同じ意味になる。
    const CapabilityTable table = CapabilityTable::buildFromQt();

    bool found = false;
    for (const FormatCapability& capability : table.encodable()) {
        if (capability.id == testFormat()) {
            found = true;
        }
    }

    REQUIRE(found);
}

TEST_CASE("the encodable set matches what Qt reports", "[plugin][capability]") {
    // 取りこぼしが無いことを集合の一致で示す。片方にしか無い形式があれば落ちる。
    const CapabilityTable table = CapabilityTable::buildFromQt();

    QSet<QString> listed;
    for (const FormatCapability& capability : table.encodable()) {
        listed.insert(capability.id.v);
    }

    REQUIRE(listed == normalizedWritableFormats());
}

// -------------------------------------------- 全 encodable 形式を横断する変換

TEST_CASE("every encodable format converts the gradient fixture", "[plugin][convert]") {
    // データ駆動。プラグインが増えれば検査対象も自動的に増える。
    const CapabilityTable table = CapabilityTable::buildFromQt();
    const QByteArray source = fixture(QStringLiteral("gradient_rgb.png"));
    const QImage original = QImage::fromData(source);

    REQUIRE_FALSE(original.isNull());

    const std::vector<FormatCapability> targets = table.encodable();
    REQUIRE_FALSE(targets.empty());

    for (const FormatCapability& target : targets) {
        INFO("target = " << formatIdToString(target.id).toStdString());

        ConversionSpec spec;
        spec.target = target.id;

        const auto result = convert(source, spec, table);
        REQUIRE(result.isOk());

        const QImage decoded = QImage::fromData(result.value().bytes);
        REQUIRE_FALSE(decoded.isNull());
        REQUIRE(decoded.size() == original.size());
    }
}
