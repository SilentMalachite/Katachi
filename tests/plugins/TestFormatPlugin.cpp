// テスト専用の QImageIOPlugin（Phase 3 T4）。
//
// docs/phases.md §4 Phase 3 の受け入れ基準 3
// 「能力表が追加コーデックを自動的に反映する（コード変更不要）」を、
// **実コーデックの有無と無関係に**検査するための土台である。
// libavif / libjxl / LibRaw が入っていない環境でも、この検査は成立する。
//
// 実在しない形式を 1 つだけ提供する。**可逆かつアルファを保つ**ように作ってあるので、
// ADR-0007 のメモリ上の往復による実測は supportsAlpha / isLossless を true と
// 判定するはずである。それを tests/plugins/extra_codec_test.cpp が確かめる。
//
// 形式は「マジック 4 バイト + 幅 + 高さ + ARGB32 の画素列」。圧縮しないので可逆であり、
// 時刻も乱数も埋め込まないので決定的である。
//
// **これはテストコードであり、フォーマット名の文字列リテラル禁止の対象外**
// （docs/spec-core.md §3）。
#include <QByteArray>
#include <QDataStream>
#include <QIODevice>
#include <QImage>
#include <QImageIOHandler>
#include <QImageIOPlugin>
#include <QtGlobal>

namespace {

// 実在しないことが名前から分かるようにする。
constexpr char testFormatName[] = "katachitest";
constexpr char testFormatMagic[] = "KTCH";
constexpr int magicSize = 4;

QByteArray magicBytes() { return QByteArray(testFormatMagic, magicSize); }

} // namespace

class TestFormatHandler final : public QImageIOHandler {
public:
    [[nodiscard]] bool canRead() const override;
    bool read(QImage* image) override;
    bool write(const QImage& image) override;
};

bool TestFormatHandler::canRead() const {
    if (device() == nullptr) {
        return false;
    }
    return device()->peek(magicSize) == magicBytes();
}

bool TestFormatHandler::read(QImage* image) {
    if (image == nullptr || device() == nullptr) {
        return false;
    }

    QDataStream stream(device());
    stream.setVersion(QDataStream::Qt_6_0);

    QByteArray magic(magicSize, '\0');
    if (stream.readRawData(magic.data(), magicSize) != magicSize || magic != magicBytes()) {
        return false;
    }

    qint32 width = 0;
    qint32 height = 0;
    stream >> width >> height;
    if (width <= 0 || height <= 0) {
        return false;
    }

    QImage decoded(width, height, QImage::Format_ARGB32);
    if (decoded.isNull()) {
        return false;
    }

    for (int row = 0; row < height; ++row) {
        for (int column = 0; column < width; ++column) {
            quint32 pixel = 0;
            stream >> pixel;
            decoded.setPixel(column, row, pixel);
        }
    }

    if (stream.status() != QDataStream::Ok) {
        return false;
    }

    *image = decoded;
    return true;
}

bool TestFormatHandler::write(const QImage& image) {
    if (device() == nullptr || image.isNull()) {
        return false;
    }

    const QImage source = image.convertToFormat(QImage::Format_ARGB32);

    QDataStream stream(device());
    stream.setVersion(QDataStream::Qt_6_0);
    stream.writeRawData(testFormatMagic, magicSize);
    stream << static_cast<qint32>(source.width()) << static_cast<qint32>(source.height());

    for (int row = 0; row < source.height(); ++row) {
        for (int column = 0; column < source.width(); ++column) {
            stream << static_cast<quint32>(source.pixel(column, row));
        }
    }

    return stream.status() == QDataStream::Ok;
}

class TestFormatPlugin final : public QImageIOPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QImageIOHandlerFactoryInterface" FILE
                          "testformat.json")

public:
    [[nodiscard]] Capabilities capabilities(QIODevice* device,
                                            const QByteArray& format) const override;
    [[nodiscard]] QImageIOHandler* create(QIODevice* device,
                                          const QByteArray& format) const override;
};

QImageIOPlugin::Capabilities TestFormatPlugin::capabilities(QIODevice* device,
                                                            const QByteArray& format) const {
    if (format == QByteArray(testFormatName)) {
        return Capabilities(CanRead | CanWrite);
    }
    if (!format.isEmpty()) {
        return {};
    }
    if (device == nullptr || !device->isOpen()) {
        return {};
    }

    Capabilities capabilities;
    if (device->isReadable() && device->peek(magicSize) == magicBytes()) {
        capabilities |= CanRead;
    }
    if (device->isWritable()) {
        capabilities |= CanWrite;
    }
    return capabilities;
}

QImageIOHandler* TestFormatPlugin::create(QIODevice* device, const QByteArray& format) const {
    // QImageIOPlugin::create() の契約は「生ポインタを返し、所有権を呼び出し側へ渡す」。
    // Qt の API がそう定めているため、ここでは new を使う（docs/cpp-conventions.md §1 の
    // 「親付き new は app 層のみ」は本プロジェクトの src/ に対する規約であり、
    // ここはテスト専用のプラグインである）。
    auto* handler = new TestFormatHandler;
    handler->setDevice(device);
    handler->setFormat(format.isEmpty() ? QByteArray(testFormatName) : format);
    return handler;
}

#include "TestFormatPlugin.moc"
