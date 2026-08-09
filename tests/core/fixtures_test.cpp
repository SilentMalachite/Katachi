// フィクスチャが T5 の前提を満たしているかを確認するテスト。
//
// 生成器（tests/fixtures/generate.cpp）自身も検証を行うが、それはビルド時の話であり、
// ここでは「テストから見て使える状態か」を確認する。フィクスチャが壊れていると
// T5 の変換テストが「convert() の不具合」に見える形で落ちるため、切り分けを先に用意する。
//
// テスト名は ASCII に限る。
#include <QByteArray>
#include <QColorSpace>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QImage>
#include <QImageIOHandler>
#include <QImageReader>
#include <QString>

#include <catch2/catch_test_macros.hpp>

namespace {

constexpr qint64 sizeLimitBytes = 50 * 1024;

QString fixturePath(const QString& name) {
    return QString::fromUtf8(KATACHI_FIXTURE_DIR) + QLatin1Char('/') + name;
}

QByteArray readFixture(const QString& name) {
    QFile file(fixturePath(name));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

} // namespace

TEST_CASE("every fixture stays under the 50KB limit", "[fixtures]") {
    // docs/phases.md §2.4 の制約を機械的に確認する。
    const QStringList names{
        QStringLiteral("gradient_rgb.png"), QStringLiteral("gradient_alpha.png"),
        QStringLiteral("with_text.png"),    QStringLiteral("with_icc.png"),
        QStringLiteral("oriented.tiff"),    QStringLiteral("not_an_image.bin")};

    for (const QString& name : names) {
        const QFileInfo info(fixturePath(name));
        INFO(name.toStdString());
        REQUIRE(info.exists());
        REQUIRE(info.size() > 0);
        REQUIRE(info.size() < sizeLimitBytes);
    }
}

TEST_CASE("gradient_rgb has no alpha channel", "[fixtures]") {
    const QImage image = QImage::fromData(readFixture(QStringLiteral("gradient_rgb.png")));

    REQUIRE_FALSE(image.isNull());
    REQUIRE_FALSE(image.hasAlphaChannel());
}

TEST_CASE("gradient_alpha carries a fully transparent pixel", "[fixtures]") {
    const QImage image = QImage::fromData(readFixture(QStringLiteral("gradient_alpha.png")));

    REQUIRE_FALSE(image.isNull());
    REQUIRE(image.hasAlphaChannel());
    REQUIRE(image.convertToFormat(QImage::Format_ARGB32).pixelColor(0, 0).alpha() == 0);
}

TEST_CASE("with_text carries text metadata", "[fixtures]") {
    // テキストは QImageReader::text() では取れない。実測（Qt 6.11.1）では
    // textKeys() が空になり、デコード後の QImage::text() からしか読めない。
    // T5 でメタデータ保持を実装するときも、読み取りはこの経路を使う。
    const QImage image = QImage::fromData(readFixture(QStringLiteral("with_text.png")));

    REQUIRE_FALSE(image.isNull());
    REQUIRE(image.text(QStringLiteral("Description")) == QStringLiteral("katachi fixture"));
}

TEST_CASE("with_icc carries a colour space", "[fixtures]") {
    const QImage image = QImage::fromData(readFixture(QStringLiteral("with_icc.png")));

    REQUIRE_FALSE(image.isNull());
    REQUIRE(image.colorSpace().isValid());
}

TEST_CASE("oriented carries orientation metadata", "[fixtures]") {
    QImageReader reader(fixturePath(QStringLiteral("oriented.tiff")));
    reader.setAutoTransform(false);
    reader.read();

    REQUIRE(reader.transformation() != QImageIOHandler::TransformationNone);
}

TEST_CASE("not_an_image cannot be decoded", "[fixtures]") {
    // ConvertError::DecodeFailed のテストが成立する前提を確認する。
    const QImage image = QImage::fromData(readFixture(QStringLiteral("not_an_image.bin")));

    REQUIRE(image.isNull());
}
