// テストフィクスチャの生成器（docs/phases.md §2.4）。
//
// 「tests/fixtures/ には自前で生成した小さな画像のみ（各 < 50KB、他者の著作物を混ぜない）。
//   生成スクリプトも tests/fixtures/generate.cpp として残す」
//
// 生成物はリポジトリにコミットせず、ビルド時にここから作る。
// 生成器と成果物が乖離しないこと、git にバイナリを置かないことを優先した。
//
// 使い方: katachi_fixture_gen <出力ディレクトリ>
//
// 作れなかった場合、あるいは意図した性質が往復で保たれなかった場合は
// 非ゼロ終了する。**黙って壊れたフィクスチャを置かない。**
// それを使う T5 のテストが「なぜか通らない」形で後から効いてくるため。
#include <QBuffer>
#include <QByteArray>
#include <QColor>
#include <QColorSpace>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QImage>
#include <QImageIOHandler>
#include <QImageReader>
#include <QImageWriter>
#include <QString>
#include <QStringList>
#include <QTextStream>

namespace {

constexpr int edge = 64;
constexpr int channelMax = 255;
constexpr qint64 sizeLimitBytes = 50 * 1024;

QTextStream& err() {
    static QTextStream stream(stderr);
    return stream;
}

QTextStream& out() {
    static QTextStream stream(stdout);
    return stream;
}

// なだらかな階調。可逆往復にも PSNR 測定にも使える。
QImage gradientRgb() {
    QImage image(edge, edge, QImage::Format_RGB32);
    for (int row = 0; row < edge; ++row) {
        for (int column = 0; column < edge; ++column) {
            const int red = (column * channelMax) / (edge - 1);
            const int green = (row * channelMax) / (edge - 1);
            const int blue = ((column + row) * channelMax) / (2 * (edge - 1));
            image.setPixelColor(column, row, QColor(red, green, blue));
        }
    }
    return image;
}

// アルファが場所によって変わる画像。完全透明の画素を必ず含む。
QImage gradientAlpha() {
    QImage image(edge, edge, QImage::Format_ARGB32);
    for (int row = 0; row < edge; ++row) {
        for (int column = 0; column < edge; ++column) {
            const int alpha = (column * channelMax) / (edge - 1);
            image.setPixelColor(column, row, QColor(channelMax - alpha, alpha, row, alpha));
        }
    }
    // 完全透明と完全不透明を確実に含める。
    image.setPixelColor(0, 0, QColor(0, 0, 0, 0));
    image.setPixelColor(edge - 1, edge - 1, QColor(channelMax, channelMax, channelMax, channelMax));
    return image;
}

bool writeImage(const QImage& image, const QString& path, const QByteArray& format,
                const QString& textKey, const QString& textValue,
                QImageIOHandler::Transformations transformation) {
    QImageWriter writer(path, format);
    if (!textKey.isEmpty()) {
        writer.setText(textKey, textValue);
    }
    if (transformation != QImageIOHandler::TransformationNone) {
        writer.setTransformation(transformation);
    }
    if (!writer.write(image)) {
        err() << "書き出しに失敗: " << path << " (" << writer.errorString() << ")\n";
        return false;
    }
    return true;
}

bool checkSize(const QString& path) {
    const QFileInfo info(path);
    if (!info.exists()) {
        err() << "生成されなかった: " << path << "\n";
        return false;
    }
    if (info.size() >= sizeLimitBytes) {
        err() << "50KB 以上ある: " << path << " (" << info.size() << " bytes)\n";
        return false;
    }
    out() << "  " << info.fileName() << "  " << info.size() << " bytes\n";
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    const QCoreApplication app(argc, argv);
    const QStringList args = QCoreApplication::arguments();
    if (args.size() != 2) {
        err() << "usage: katachi_fixture_gen <output-dir>\n";
        return 2;
    }

    const QString dir = args.at(1);
    if (!QDir().mkpath(dir)) {
        err() << "出力ディレクトリを作れない: " << dir << "\n";
        return 1;
    }
    out() << "フィクスチャを生成: " << dir << "\n";

    bool ok = true;

    // 1. 階調のみ、アルファ無し。ラウンドトリップ / PSNR / 決定性に使う。
    const QString gradientPath = dir + QStringLiteral("/gradient_rgb.png");
    ok = ok && writeImage(gradientRgb(), gradientPath, "png", {}, {},
                          QImageIOHandler::TransformationNone);
    ok = ok && checkSize(gradientPath);

    // 2. アルファ有り。docs/spec-core.md §4 のアルファ表に使う。
    const QString alphaPath = dir + QStringLiteral("/gradient_alpha.png");
    ok = ok &&
         writeImage(gradientAlpha(), alphaPath, "png", {}, {}, QImageIOHandler::TransformationNone);
    ok = ok && checkSize(alphaPath);

    // 3. テキスト metadata 付き。MetadataPolicy の検証に使う。
    const QString textPath = dir + QStringLiteral("/with_text.png");
    ok = ok && writeImage(gradientRgb(), textPath, "png", QStringLiteral("Description"),
                          QStringLiteral("katachi fixture"), QImageIOHandler::TransformationNone);
    ok = ok && checkSize(textPath);

    // 4. ICC プロファイル付き。IccPolicy の検証に使う。
    const QString iccPath = dir + QStringLiteral("/with_icc.png");
    {
        QImage tagged = gradientRgb();
        tagged.setColorSpace(QColorSpace(QColorSpace::DisplayP3));
        ok = ok && writeImage(tagged, iccPath, "png", {}, {}, QImageIOHandler::TransformationNone);
        ok = ok && checkSize(iccPath);
    }

    // 5. 向き metadata 付き。
    //
    // **JPEG では作れない。** 実測（Qt 6.11.1）では、JPEG の書き出しに
    // setTransformation() を与えても EXIF は書かれず、回転がピクセルへ焼き込まれる
    // （16x8 の入力が 8x16 になり、ファイルに Exif マーカーも無い）。
    // これは QImageWriter::setTransformation の
    // "If transformation metadata is not supported by the image format,
    //  the transform is applied before writing" に対応する挙動。
    //
    // TIFF は向きを metadata として保持する（読み戻して Rotate90 が取れ、寸法も変わらない）。
    const QString orientedPath = dir + QStringLiteral("/oriented.tiff");
    ok = ok && writeImage(gradientRgb(), orientedPath, "tiff", {}, {},
                          QImageIOHandler::TransformationRotate90);
    ok = ok && checkSize(orientedPath);

    // 6. 索引色（パレット）画像。
    //    テキスト除去でカラーテーブルを取りこぼすと見た目が壊れるため、その検出に使う。
    const QString indexedPath = dir + QStringLiteral("/indexed.png");
    {
        QImage indexed(edge, edge, QImage::Format_Indexed8);
        indexed.setColorCount(4);
        indexed.setColor(0, qRgb(channelMax, 0, 0));
        indexed.setColor(1, qRgb(0, channelMax, 0));
        indexed.setColor(2, qRgb(0, 0, channelMax));
        indexed.setColor(3, qRgb(channelMax, channelMax, 0));
        for (int row = 0; row < edge; ++row) {
            for (int column = 0; column < edge; ++column) {
                indexed.setPixel(column, row, static_cast<uint>((column / 16 + row / 16) % 4));
            }
        }
        indexed.setText(QStringLiteral("Description"), QStringLiteral("katachi fixture"));
        ok = ok &&
             writeImage(indexed, indexedPath, "png", {}, {}, QImageIOHandler::TransformationNone);
        ok = ok && checkSize(indexedPath);
    }

    // 7. 画像でないバイト列。ConvertError::DecodeFailed の検証に使う。
    const QString brokenPath = dir + QStringLiteral("/not_an_image.bin");
    {
        QFile broken(brokenPath);
        if (!broken.open(QIODevice::WriteOnly)) {
            err() << "書き出しに失敗: " << brokenPath << "\n";
            ok = false;
        } else {
            broken.write(QByteArray(64, '\x01'));
            broken.close();
            ok = ok && checkSize(brokenPath);
        }
    }

    if (!ok) {
        return 1;
    }

    // --- 生成物が意図した性質を実際に持っているかを検証する ---
    out() << "検証:\n";

    {
        QImageReader reader(alphaPath);
        const QImage decoded = reader.read();
        if (decoded.isNull() || !decoded.hasAlphaChannel()) {
            err() << "gradient_alpha.png がアルファを保持していない\n";
            return 1;
        }
        out() << "  gradient_alpha.png: アルファあり\n";
    }
    {
        QImageReader reader(gradientPath);
        const QImage decoded = reader.read();
        if (decoded.isNull() || decoded.hasAlphaChannel()) {
            err() << "gradient_rgb.png がアルファを持ってしまっている\n";
            return 1;
        }
        out() << "  gradient_rgb.png: アルファなし\n";
    }
    {
        // テキストの読み取りは QImage::text() を使う。
        // QImageReader::text() は取れる場合と取れない場合があり、
        // tEXt チャンクが IDAT の前か後か、read() の前か後かで結果が変わる。
        // QImage::text() は取りこぼしが無い。
        // 書き出しは QImageWriter::setText() / QImage::setText() のどちらでも保持される。
        // libpng がテキスト付き PNG の読み込みで "Read Error" を標準エラーへ出すが、
        // read() は成功し内容も正しい。無害な雑音として扱う。
        QImageReader reader(textPath);
        const QImage decoded = reader.read();
        if (decoded.isNull() ||
            decoded.text(QStringLiteral("Description")) != QStringLiteral("katachi fixture")) {
            err() << "with_text.png のテキスト metadata が保持されていない\n";
            return 1;
        }
        out() << "  with_text.png: テキスト metadata あり\n";
    }
    {
        QImageReader reader(iccPath);
        const QImage decoded = reader.read();
        if (decoded.isNull() || !decoded.colorSpace().isValid()) {
            err() << "with_icc.png の ICC プロファイルが保持されていない\n";
            return 1;
        }
        out() << "  with_icc.png: ICC プロファイルあり\n";
    }
    {
        QImageReader reader(orientedPath);
        reader.setAutoTransform(false);
        reader.read();
        if (reader.transformation() == QImageIOHandler::TransformationNone) {
            err() << "oriented.tiff の向き metadata が保持されていない\n";
            return 1;
        }
        out() << "  oriented.tiff: 向き metadata あり\n";
    }

    out() << "すべて検証済み\n";
    return 0;
}
