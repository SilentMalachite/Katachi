#pragma once

// 本アプリの心臓部（docs/spec-core.md §2）。
//
// 純粋関数：ファイルシステム・時刻・グローバル状態・乱数に触れない。
// 同一入力に対して常に同一出力（バイト列）を返す。
//
// **テンプレートにしない。** 差し替えたいのは能力表の「中身」であって型ではなく、
// CapabilityTable::fromCapabilities() で足りる（docs/cpp-conventions.md §2.3）。
// 実装は Converter.cpp に閉じる。
//
// この性質を壊す変更（キャッシュ、静的変数の導入等）は禁止。
// 「性能のため」も理由にならない（docs/spec-core.md §6）。

#include "core/CapabilityTable.hpp"
#include "core/ConversionSpec.hpp"
#include "core/ConvertError.hpp"
#include "core/Result.hpp"

#include <QByteArray>

#include <vector>

namespace katachi::core {

// 成功値。警告はエラーではないため E 側ではなくここに載せる（ADR-0004）。
// QByteArray も std::vector も nothrow move 構築可能なので ResultValue を満たす。
struct ConversionOutput {
    QByteArray bytes;
    std::vector<ConvertWarning> warnings;
};

// noexcept と確保失敗の扱いは ADR-0002 に従う。
[[nodiscard]] Result<ConversionOutput, ConvertError>
convert(const QByteArray& source, const ConversionSpec& spec, const CapabilityTable& caps) noexcept;

} // namespace katachi::core
