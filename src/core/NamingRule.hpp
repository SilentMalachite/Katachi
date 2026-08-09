#pragma once

// 出力ファイル名の生成（docs/spec-core.md §5）。純粋関数。
//
// **名前を組み立てるだけで、衝突の解決はしない**（ADR-0005）。
// 衝突判定には出力先の実在確認、すなわちファイルシステム参照が要り、
// core では禁止されている。衝突ポリシー（Overwrite / Skip / Rename）の適用は
// Phase 2 の src/io 層に置く。

#include "core/Result.hpp"

#include <QString>

#include <cstdint>

namespace katachi::core {

// 基底型を明示するのは clang-tidy の performance-enum-size による。
enum class NamingError : std::uint8_t {
    EmptyPattern,       // pattern が空
    UnknownPlaceholder, // {} 内が既知の名前でない、または閉じていない
    InvalidIndexSpec,   // {index:...} の桁指定が不正
    EmptyResult,        // 展開結果が空になった
};

// 書式と拡張子を強い型で分ける。どちらも中身は QString なので、素の引数で並べると
// 呼び出し側が取り違えられる（clang-tidy: bugprone-easily-swappable-parameters）。
// FormatId と同じ「強い型付き文字列」の考え方（docs/spec-core.md §2.1）で塞ぐ。
struct NamePattern {
    QString v;
};

struct NameExtension {
    QString v;
};

// 差し込める名前は {name} / {index} / {ext} の 3 つ。
// {index} は :N を付けて最小桁数を指定できる（0 詰め）。例: {index:03}
//
// 例: resolveOutputName("photo", 1, {"{name}_{index:03}.{ext}"}, {"png"}) -> "photo_001.png"
//
// noexcept と確保失敗の扱いは ADR-0002 に従う。
[[nodiscard]] Result<QString, NamingError>
resolveOutputName(const QString& sourceBaseName, int index, const NamePattern& pattern,
                  const NameExtension& extension) noexcept;

} // namespace katachi::core
