#include "core/Converter.hpp"

#include "core/CapabilityTable.hpp"
#include "core/ConversionSpec.hpp"
#include "core/ConvertError.hpp"
#include "core/FormatId.hpp"
#include "core/Result.hpp"

#include <QBuffer>
#include <QByteArray>
#include <QColorSpace>
#include <QIODevice>
#include <QImage>
#include <QImageIOHandler>
#include <QImageReader>
#include <QImageWriter>
#include <QList>
#include <QPainter>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <Qt>
#include <QtTypes>

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

namespace katachi::core {
namespace {

using ConvertResult = Result<ConversionOutput, ConvertError>;

// アルファ合成。プリマルチプライド前提で行わない（docs/spec-core.md §4）。
// Format_ARGB32 に正規化してから、指定色の上に CompositionMode_SourceOver で描く。
//
// キャンバスを RGB32 でなく ARGB32 にしているのは合成の丸め精度のため。
// 実測（16,777,216 サンプル）では、描画先が RGB32 / ARGB32_Premultiplied だと
// 二重丸めにより約 24% の画素で理論値から ±1 ずれる（最大誤差 0.98 LSB）。
// ARGB32 なら最大誤差 0.51 LSB に収まる。最後に RGB32 へ落として不透明にする。
[[nodiscard]] QImage flattenOnto(const QImage& source, const QColor& background) {
    const QImage normalized = source.convertToFormat(QImage::Format_ARGB32);

    QImage canvas(normalized.size(), QImage::Format_ARGB32);
    canvas.fill(background);
    {
        QPainter painter(&canvas);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.drawImage(0, 0, normalized);
    }
    // QPainter は描画先へ色空間を伝播しない。明示的に引き継がないと
    // 合成しただけで ICC が消える（IccPolicy::Embed の指定が無視される）。
    canvas.setColorSpace(source.colorSpace());

    return canvas.convertToFormat(QImage::Format_RGB32);
}

// テキストを持たない同じ画素の画像を作る。
// QImage には「全テキストを消す」公開 API が無いため、生ビットから作り直す。
// setText(key, QString()) では消えない（キーが残り、空値のチャンクが書き出される）。
//
// **生ビットからの作り直しはテキスト以外の付随情報も落とす。**
// 特にカラーテーブルを戻さないと、索引色画像は画素値が同じでも見た目が別物になる。
// 落ちるものを明示的に戻す。
[[nodiscard]] QImage withoutText(const QImage& source) {
    const QImage view(source.constBits(), source.width(), source.height(), source.bytesPerLine(),
                      source.format());
    QImage stripped = view.copy();

    stripped.setColorTable(source.colorTable());
    stripped.setColorSpace(source.colorSpace());
    stripped.setDotsPerMeterX(source.dotsPerMeterX());
    stripped.setDotsPerMeterY(source.dotsPerMeterY());
    stripped.setDevicePixelRatio(source.devicePixelRatio());

    return stripped;
}

// Qt が読み取り時に注入する内部キーだけを落とす。
//
// 例: ICO を読むと _q_icoOrigDepth が QImage のテキストへ入る（Phase 1 T5 追補で実測）。
// PreserveSupported の意味は「利用者の metadata を保つ」ことであって、
// Qt の内部情報を出力へ漏らすことではない。
//
// 前置きの _q_ は Qt が内部用の名前に使う接頭辞。利用者のキーは畳まない。
[[nodiscard]] QImage withoutInternalText(const QImage& source) {
    // QStringList と綴らないのは clang-tidy の misc-include-cleaner による。
    // Qt 6 の QStringList は qcontainerfwd.h で宣言された QList<QString> の別名であり、
    // <QStringList> を include しても「提供ヘッダが未 include」と判定される。
    // 型そのものを綴って <QList> を直接 include する。
    const QList<QString> keys = source.textKeys();
    const auto isInternal = [](const QString& key) { return key.startsWith(QStringView(u"_q_")); };

    if (!std::ranges::any_of(keys, isInternal)) {
        // 内部キーが無ければ作り直さない。無駄な確保を避ける（ADR-0002）。
        return source;
    }

    QImage cleaned = withoutText(source);
    for (const QString& key : keys) {
        if (!isInternal(key)) {
            cleaned.setText(key, source.text(key));
        }
    }

    return cleaned;
}

} // namespace

Result<ConversionOutput, ConvertError> convert(const QByteArray& source, const ConversionSpec& spec,
                                               const CapabilityTable& caps) noexcept {
    // 処理順序は決定性のために固定する。入れ替えない。
    //   空判定 -> 出力形式の妥当性 -> maxPixels 事前判定 -> 復号
    //   -> リサイズ -> アルファ -> メタデータ / ICC -> 符号化

    if (source.isEmpty()) {
        return ConvertResult::err(ConvertError::EmptyInput);
    }

    // 出力形式の妥当性は画像を読む前に判定できる。無駄な復号を避ける。
    const std::optional<FormatCapability> target = caps.find(spec.target);
    if (!target.has_value() || !target->canEncode) {
        return ConvertResult::err(ConvertError::UnsupportedTarget);
    }

    QByteArray input = source;
    QBuffer origin(&input);
    if (!origin.open(QIODevice::ReadOnly)) {
        return ConvertResult::err(ConvertError::DecodeFailed);
    }

    QImageReader reader(&origin);
    // 向きは metadata として持ち回る。ここで画素へ適用してしまうと
    // 保持も除去も選べなくなる。
    reader.setAutoTransform(false);

    // ADR-0002: maxPixels の判定は復号より前に行う。
    // ここで弾かないと、上限を超える入力で確保が走り std::terminate しうる。
    const QSize declared = reader.size();
    if (declared.isValid()) {
        const qint64 pixels = static_cast<qint64>(declared.width()) * declared.height();
        if (pixels > spec.maxPixels) {
            return ConvertResult::err(ConvertError::ImageTooLarge);
        }
    }

    const QImageIOHandler::Transformations transformation = reader.transformation();

    QImage image = reader.read();
    if (image.isNull()) {
        return ConvertResult::err(ConvertError::DecodeFailed);
    }

    // 復号後の実寸でも上限を確認する。ヘッダの寸法が信用できない形式があるため。
    {
        const qint64 pixels = static_cast<qint64>(image.width()) * image.height();
        if (pixels > spec.maxPixels) {
            return ConvertResult::err(ConvertError::ImageTooLarge);
        }
    }

    if (spec.resize.has_value()) {
        // 補間は Qt::SmoothTransformation 固定、アスペクト比は常に保持
        // （docs/phases.md §5.1、docs/spec-core.md §2）。
        image = image.scaled(*spec.resize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    std::vector<ConvertWarning> warnings;

    // docs/spec-core.md §4 の表をそのまま実装する。
    if (image.hasAlphaChannel() && !target->supportsAlpha) {
        switch (spec.alpha) {
        case AlphaPolicy::Reject:
            return ConvertResult::err(ConvertError::AlphaLossNotAllowed);
        case AlphaPolicy::Preserve:
            warnings.push_back(ConvertWarning::AlphaFlattenedFallback);
            image = flattenOnto(image, spec.flattenColor);
            break;
        case AlphaPolicy::Flatten:
            image = flattenOnto(image, spec.flattenColor);
            break;
        }
    }

    if (spec.metadata == MetadataPolicy::StripAll) {
        image = withoutText(image);
    } else if (spec.metadata == MetadataPolicy::PreserveSupported) {
        image = withoutInternalText(image);
    }

    if (spec.icc == IccPolicy::Strip) {
        image.setColorSpace(QColorSpace());
    }

    QByteArray encoded;
    QBuffer sink(&encoded);
    if (!sink.open(QIODevice::WriteOnly)) {
        return ConvertResult::err(ConvertError::EncodeFailed);
    }

    QImageWriter writer(&sink, formatIdToString(spec.target).toUtf8());
    if (target->supportsQuality) {
        writer.setQuality(std::clamp(spec.quality, minQuality, maxQuality));
    }
    if (spec.metadata == MetadataPolicy::PreserveSupported) {
        writer.setTransformation(transformation);
    }

    if (!writer.write(image)) {
        return ConvertResult::err(ConvertError::EncodeFailed);
    }
    sink.close();

    return ConvertResult::ok(
        ConversionOutput{.bytes = std::move(encoded), .warnings = std::move(warnings)});
}

} // namespace katachi::core
