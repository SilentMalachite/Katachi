// convert() のテスト（docs/phases.md §2.2 の全種別）。
//
// 期待値は docs/spec-core.md から導いている。実装の都合で緩めない。
// テスト名は ASCII に限る。
#include "core/CapabilityTable.hpp"
#include "core/ConversionSpec.hpp"
#include "core/ConvertError.hpp"
#include "core/Converter.hpp"
#include "core/FormatId.hpp"

#include <QBuffer>
#include <QByteArray>
#include <QColor>
#include <QColorSpace>
#include <QFile>
#include <QIODevice>
#include <QImage>
#include <QImageIOHandler>
#include <QImageReader>
#include <QSize>
#include <QString>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using katachi::core::AlphaPolicy;
using katachi::core::CapabilityTable;
using katachi::core::ConversionSpec;
using katachi::core::convert;
using katachi::core::ConvertError;
using katachi::core::ConvertWarning;
using katachi::core::FormatCapability;
using katachi::core::formatIdFromString;
using katachi::core::IccPolicy;
using katachi::core::MetadataPolicy;

namespace {

QByteArray fixture(const QString& name) {
    QFile file(QString::fromUtf8(KATACHI_FIXTURE_DIR) + QLatin1Char('/') + name);
    REQUIRE(file.open(QIODevice::ReadOnly));
    return file.readAll();
}

QImage decode(const QByteArray& bytes) { return QImage::fromData(bytes); }

const CapabilityTable& qtTable() {
    static const CapabilityTable table = CapabilityTable::buildFromQt();
    return table;
}

ConversionSpec specFor(const QString& target) {
    ConversionSpec spec;
    spec.target = formatIdFromString(target);
    return spec;
}

// 非可逆変換の劣化量。docs/phases.md §2.2 はバイト比較でなく PSNR で見ると定める。
double psnr(const QImage& lhs, const QImage& rhs) {
    const QImage left = lhs.convertToFormat(QImage::Format_RGB32);
    const QImage right = rhs.convertToFormat(QImage::Format_RGB32);
    REQUIRE(left.size() == right.size());

    double squaredError = 0.0;
    for (int row = 0; row < left.height(); ++row) {
        for (int column = 0; column < left.width(); ++column) {
            const QColor a = left.pixelColor(column, row);
            const QColor b = right.pixelColor(column, row);
            for (const int diff : {a.red() - b.red(), a.green() - b.green(), a.blue() - b.blue()}) {
                squaredError += static_cast<double>(diff) * diff;
            }
        }
    }
    const double count = static_cast<double>(left.width()) * left.height() * 3.0;
    const double meanSquaredError = squaredError / count;
    if (meanSquaredError == 0.0) {
        return 1000.0; // 完全一致。上限として扱う。
    }
    return 10.0 * std::log10((255.0 * 255.0) / meanSquaredError);
}

} // namespace

// ---------------------------------------------------------------- エラー網羅
// docs/phases.md §2.2: ConvertError の全列挙値に、それを発生させるテストが 1 つ以上ある。

TEST_CASE("convert rejects empty input", "[core][convert][error]") {
    const auto result = convert(QByteArray{}, specFor(QStringLiteral("png")), qtTable());

    REQUIRE_FALSE(result.isOk());
    REQUIRE(result.error() == ConvertError::EmptyInput);
}

TEST_CASE("convert reports decode failure for non image bytes", "[core][convert][error]") {
    const auto result = convert(fixture(QStringLiteral("not_an_image.bin")),
                                specFor(QStringLiteral("png")), qtTable());

    REQUIRE_FALSE(result.isOk());
    REQUIRE(result.error() == ConvertError::DecodeFailed);
}

TEST_CASE("convert reports unsupported target for an unknown format", "[core][convert][error]") {
    const auto result = convert(fixture(QStringLiteral("gradient_rgb.png")),
                                specFor(QStringLiteral("nosuchformat")), qtTable());

    REQUIRE_FALSE(result.isOk());
    REQUIRE(result.error() == ConvertError::UnsupportedTarget);
}

TEST_CASE("convert reports unsupported target when the format cannot encode",
          "[core][convert][error]") {
    FormatCapability decodeOnly;
    decodeOnly.id = formatIdFromString(QStringLiteral("png"));
    decodeOnly.canDecode = true;
    decodeOnly.canEncode = false;
    const CapabilityTable table = CapabilityTable::fromCapabilities({decodeOnly});

    const auto result =
        convert(fixture(QStringLiteral("gradient_rgb.png")), specFor(QStringLiteral("png")), table);

    REQUIRE_FALSE(result.isOk());
    REQUIRE(result.error() == ConvertError::UnsupportedTarget);
}

TEST_CASE("convert reports encode failure when the table claims a bogus format is encodable",
          "[core][convert][error]") {
    // 能力表が「書ける」と言っても Qt が書けなければ EncodeFailed になる。
    FormatCapability bogus;
    bogus.id = formatIdFromString(QStringLiteral("nosuchformat"));
    bogus.canDecode = true;
    bogus.canEncode = true;
    const CapabilityTable table = CapabilityTable::fromCapabilities({bogus});

    const auto result = convert(fixture(QStringLiteral("gradient_rgb.png")),
                                specFor(QStringLiteral("nosuchformat")), table);

    REQUIRE_FALSE(result.isOk());
    REQUIRE(result.error() == ConvertError::EncodeFailed);
}

TEST_CASE("convert rejects images beyond maxPixels", "[core][convert][error]") {
    // ADR-0002: maxPixels の判定は復号より前に行う。
    ConversionSpec spec = specFor(QStringLiteral("png"));
    spec.maxPixels = 1;

    const auto result = convert(fixture(QStringLiteral("gradient_rgb.png")), spec, qtTable());

    REQUIRE_FALSE(result.isOk());
    REQUIRE(result.error() == ConvertError::ImageTooLarge);
}

// ------------------------------------------------------- アルファ処理（§4 全行）

TEST_CASE("alpha survives when the target supports alpha", "[core][convert][alpha]") {
    // §4 1 行目: アルファあり / 対応 / 任意 -> そのまま保持
    const auto result = convert(fixture(QStringLiteral("gradient_alpha.png")),
                                specFor(QStringLiteral("png")), qtTable());

    REQUIRE(result.isOk());
    REQUIRE(result.value().warnings.empty());
    const QImage decoded = decode(result.value().bytes);
    REQUIRE(decoded.hasAlphaChannel());
    REQUIRE(decoded.convertToFormat(QImage::Format_ARGB32).pixelColor(0, 0).alpha() == 0);
}

TEST_CASE("preserve falls back to flatten and warns when the target lacks alpha",
          "[core][convert][alpha]") {
    // §4 2 行目: アルファあり / 非対応 / Preserve
    //   -> Flatten にフォールバックし、warnings に AlphaFlattenedFallback を 1 件積む
    ConversionSpec spec = specFor(QStringLiteral("jpeg"));
    spec.alpha = AlphaPolicy::Preserve;

    const auto result = convert(fixture(QStringLiteral("gradient_alpha.png")), spec, qtTable());

    REQUIRE(result.isOk());
    REQUIRE(result.value().warnings.size() == 1);
    REQUIRE(result.value().warnings.front() == ConvertWarning::AlphaFlattenedFallback);
    REQUIRE_FALSE(decode(result.value().bytes).hasAlphaChannel());
}

TEST_CASE("flatten composites onto the flatten colour", "[core][convert][alpha]") {
    // §4 3 行目: アルファあり / 非対応 / Flatten -> flattenColor で合成。警告は出ない。
    ConversionSpec spec = specFor(QStringLiteral("bmp"));
    spec.alpha = AlphaPolicy::Flatten;
    spec.flattenColor = QColor(0, 0, 255);

    const auto result = convert(fixture(QStringLiteral("gradient_alpha.png")), spec, qtTable());

    REQUIRE(result.isOk());
    REQUIRE(result.value().warnings.empty());
    const QImage decoded = decode(result.value().bytes).convertToFormat(QImage::Format_RGB32);
    // (0,0) は完全透明なので、合成後は flattenColor そのものになる。
    REQUIRE(decoded.pixelColor(0, 0) == QColor(0, 0, 255));
}

TEST_CASE("reject refuses to lose alpha", "[core][convert][alpha]") {
    // §4 4 行目: アルファあり / 非対応 / Reject -> ConvertError::AlphaLossNotAllowed
    ConversionSpec spec = specFor(QStringLiteral("jpeg"));
    spec.alpha = AlphaPolicy::Reject;

    const auto result = convert(fixture(QStringLiteral("gradient_alpha.png")), spec, qtTable());

    REQUIRE_FALSE(result.isOk());
    REQUIRE(result.error() == ConvertError::AlphaLossNotAllowed);
}

TEST_CASE("images without alpha pass through every policy", "[core][convert][alpha]") {
    // §4 5 行目: アルファなし / 任意 / 任意 -> そのまま。警告は出ない。
    for (const AlphaPolicy policy :
         {AlphaPolicy::Preserve, AlphaPolicy::Flatten, AlphaPolicy::Reject}) {
        ConversionSpec spec = specFor(QStringLiteral("png"));
        spec.alpha = policy;

        const auto result = convert(fixture(QStringLiteral("gradient_rgb.png")), spec, qtTable());

        REQUIRE(result.isOk());
        REQUIRE(result.value().warnings.empty());
    }
}

// ------------------------------------------------------------ ラウンドトリップ

TEST_CASE("png to png round trip is pixel exact", "[core][convert][roundtrip]") {
    const QByteArray source = fixture(QStringLiteral("gradient_rgb.png"));
    const auto result = convert(source, specFor(QStringLiteral("png")), qtTable());

    REQUIRE(result.isOk());
    REQUIRE(decode(result.value().bytes).convertToFormat(QImage::Format_RGB32) ==
            decode(source).convertToFormat(QImage::Format_RGB32));
}

TEST_CASE("png to bmp round trip is pixel exact", "[core][convert][roundtrip]") {
    const QByteArray source = fixture(QStringLiteral("gradient_rgb.png"));
    const auto result = convert(source, specFor(QStringLiteral("bmp")), qtTable());

    REQUIRE(result.isOk());
    REQUIRE(decode(result.value().bytes).convertToFormat(QImage::Format_RGB32) ==
            decode(source).convertToFormat(QImage::Format_RGB32));
}

TEST_CASE("png to jpeg keeps psnr above 35 db", "[core][convert][lossy]") {
    // docs/phases.md §2.2: 非可逆はデコードし直して PSNR >= 35dB。バイト比較しない。
    const QByteArray source = fixture(QStringLiteral("gradient_rgb.png"));
    const auto result = convert(source, specFor(QStringLiteral("jpeg")), qtTable());

    REQUIRE(result.isOk());
    REQUIRE(psnr(decode(source), decode(result.value().bytes)) >= 35.0);
}

// ------------------------------------------------------------------ 決定性

TEST_CASE("convert is deterministic for the same input and spec", "[core][convert][determinism]") {
    // docs/phases.md §2.2: 同じ入力・同じ Spec で 2 回変換 -> バイト列が完全一致。
    const QByteArray source = fixture(QStringLiteral("gradient_rgb.png"));

    for (const QString& target : {QStringLiteral("png"), QStringLiteral("jpeg"),
                                  QStringLiteral("bmp"), QStringLiteral("tiff")}) {
        const auto first = convert(source, specFor(target), qtTable());
        const auto second = convert(source, specFor(target), qtTable());

        INFO(target.toStdString());
        REQUIRE(first.isOk());
        REQUIRE(second.isOk());
        REQUIRE(first.value().bytes == second.value().bytes);
    }
}

// ------------------------------------------------------------------ リサイズ

TEST_CASE("resize keeps the aspect ratio", "[core][convert][resize]") {
    // docs/spec-core.md §2: アスペクト比は常に保持。
    ConversionSpec spec = specFor(QStringLiteral("png"));
    spec.resize = QSize(32, 16);

    const auto result = convert(fixture(QStringLiteral("gradient_rgb.png")), spec, qtTable());

    REQUIRE(result.isOk());
    const QImage decoded = decode(result.value().bytes);
    // 入力は 64x64 の正方形。32x16 に収めるとアスペクト比保持で 16x16 になる。
    REQUIRE(decoded.size() == QSize(16, 16));
}

// ------------------------------------------------------------ メタデータ / ICC

TEST_CASE("preserve supported keeps text metadata", "[core][convert][metadata]") {
    ConversionSpec spec = specFor(QStringLiteral("png"));
    spec.metadata = MetadataPolicy::PreserveSupported;

    const auto result = convert(fixture(QStringLiteral("with_text.png")), spec, qtTable());

    REQUIRE(result.isOk());
    REQUIRE(decode(result.value().bytes).text(QStringLiteral("Description")) ==
            QStringLiteral("katachi fixture"));
}

TEST_CASE("strip all removes text metadata", "[core][convert][metadata]") {
    ConversionSpec spec = specFor(QStringLiteral("png"));
    spec.metadata = MetadataPolicy::StripAll;

    const auto result = convert(fixture(QStringLiteral("with_text.png")), spec, qtTable());

    REQUIRE(result.isOk());
    REQUIRE(decode(result.value().bytes).textKeys().isEmpty());
}

TEST_CASE("embed keeps the colour space", "[core][convert][metadata]") {
    ConversionSpec spec = specFor(QStringLiteral("png"));
    spec.icc = IccPolicy::Embed;

    const auto result = convert(fixture(QStringLiteral("with_icc.png")), spec, qtTable());

    REQUIRE(result.isOk());
    REQUIRE(decode(result.value().bytes).colorSpace().isValid());
}

TEST_CASE("strip removes the colour space", "[core][convert][metadata]") {
    ConversionSpec spec = specFor(QStringLiteral("png"));
    spec.icc = IccPolicy::Strip;

    const auto result = convert(fixture(QStringLiteral("with_icc.png")), spec, qtTable());

    REQUIRE(result.isOk());
    REQUIRE_FALSE(decode(result.value().bytes).colorSpace().isValid());
}

TEST_CASE("preserve supported keeps orientation metadata", "[core][convert][metadata]") {
    // 向きを metadata として保持できるのは TIFF（JPEG は焼き込みになる）。
    ConversionSpec spec = specFor(QStringLiteral("tiff"));
    spec.metadata = MetadataPolicy::PreserveSupported;

    const auto result = convert(fixture(QStringLiteral("oriented.tiff")), spec, qtTable());

    REQUIRE(result.isOk());

    QBuffer buffer;
    QByteArray bytes = result.value().bytes;
    buffer.setBuffer(&bytes);
    REQUIRE(buffer.open(QIODevice::ReadOnly));
    QImageReader reader(&buffer);
    reader.setAutoTransform(false);
    reader.read();
    REQUIRE(reader.transformation() != QImageIOHandler::TransformationNone);
}

TEST_CASE("strip all removes orientation metadata", "[core][convert][metadata]") {
    ConversionSpec spec = specFor(QStringLiteral("tiff"));
    spec.metadata = MetadataPolicy::StripAll;

    const auto result = convert(fixture(QStringLiteral("oriented.tiff")), spec, qtTable());

    REQUIRE(result.isOk());

    QBuffer buffer;
    QByteArray bytes = result.value().bytes;
    buffer.setBuffer(&bytes);
    REQUIRE(buffer.open(QIODevice::ReadOnly));
    QImageReader reader(&buffer);
    reader.setAutoTransform(false);
    reader.read();
    REQUIRE(reader.transformation() == QImageIOHandler::TransformationNone);
}
