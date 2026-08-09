#pragma once

// 変換の失敗理由と、成功したが指定どおりに処理できなかったことの通知。
//
// ConvertWarning を専用ファイルに分けないのは docs/agent-protocol.md §6
// 「ファイルを新規作成する前に、既存ファイルへの追記で済まないか検討する」に従ったもの。
// どちらも convert() の結果を説明する列挙であり、同じ場所にある方が対応関係を追いやすい。
//
// 基底型を明示するのは clang-tidy の performance-enum-size が要求するため。
// docs/phases.md §3 は performance-* を必須で有効にしており、除外できるのは
// cppcoreguidelines-pro-bounds-* と modernize-use-trailing-return-type の 2 種のみ。
// 抑制コメントによる除外も CLAUDE.md で禁止されている
// （その語をここに書くと不変条件スキャナ INV6 自身が検出する。実際に検出された）。
// 列挙子の顔ぶれは docs/spec-core.md §2 のままで、意味は変えていない。

#include <cstdint>

namespace katachi::core {

enum class ConvertError : std::uint8_t {
    EmptyInput,
    DecodeFailed,
    UnsupportedTarget,
    EncodeFailed,
    AlphaLossNotAllowed, // アルファ画像を非対応形式へ Reject 指定で変換
    ImageTooLarge,       // 上限は ConversionSpec::maxPixels
};

// エラーではないため Result の E 側ではなく ConversionOutput に載せる（ADR-0004）。
// 文言は表示層（src/app）の責務。core に文字列リテラルは置けない（不変条件 INV3A）。
enum class ConvertWarning : std::uint8_t {
    // docs/spec-core.md §4 の 3 行目:
    // アルファあり → 非対応形式 → Preserve のとき Flatten にフォールバックした。
    AlphaFlattenedFallback,
};

} // namespace katachi::core
