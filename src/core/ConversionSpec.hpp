#pragma once

// 変換の指定（docs/spec-core.md §2）。
//
// 値型。メンバは非 const（集成体初期化とコピー代入を壊さないため）だが、
// 「生成後に書き換えない」ことを規約とする。受け渡しは常に const 参照。

#include "core/FormatId.hpp"

#include <QColor>
#include <QSize>
#include <QtTypes>

#include <cstdint>
#include <optional>

namespace katachi::core {

// 基底型を明示するのは clang-tidy の performance-enum-size による。
enum class AlphaPolicy : std::uint8_t { Preserve, Flatten, Reject };

// PreserveSupported は「Qt が扱える範囲」＝ 向き / テキスト / ICC を保持する。
// EXIF 全体の保持は Qt 単体では不可能（ADR-0003）。
enum class MetadataPolicy : std::uint8_t { PreserveSupported, StripAll };

enum class IccPolicy : std::uint8_t { Embed, Strip };

// 既定値を名前付き定数にしているのは clang-tidy の
// readability-magic-numbers / cppcoreguidelines-avoid-magic-numbers による。
inline constexpr int defaultQuality = 90;
inline constexpr int minQuality = 0;
inline constexpr int maxQuality = 100;

// 16384 x 16384。これを超える入力は復号前に弾く（ADR-0002）。
inline constexpr qint64 defaultMaxPixels = 268'435'456;

struct ConversionSpec {
    FormatId target;                            // 文字列でなく型で持つ（§2.1）
    int quality = defaultQuality;               // minQuality..maxQuality
    std::optional<QSize> resize = std::nullopt; // アスペクト比は常に保持
    AlphaPolicy alpha = AlphaPolicy::Preserve;  //
    QColor flattenColor = Qt::white;            //
    MetadataPolicy metadata = MetadataPolicy::PreserveSupported;
    IccPolicy icc = IccPolicy::Embed;
    qint64 maxPixels = defaultMaxPixels;
};

} // namespace katachi::core
