#pragma once

// 実行時に生成する能力表（docs/spec-core.md §3）。
//
// CLAUDE.md: 対応フォーマットはハードコードせず、実行時に QImageReader / QImageWriter から
// 能力表を生成する。static でもシングルトンでもなく、明示的に生成して注入する。
//
// このヘッダは core/Concepts.hpp を include しない。
// Concepts.hpp の CapabilitySource が FormatCapability を参照するため、
// 逆向きに include すると循環する。契約の static_assert は CapabilityTable.cpp に置く。

#include "core/FormatId.hpp"

#include <QStringList>

#include <optional>
#include <vector>

namespace katachi::core {

struct FormatCapability {
    FormatId id;
    // Qt が報告した名称の和集合。別名が正規化で 1 件へ畳まれた場合、
    // 畳まれた名称がすべてここに残る（ADR-0006）。
    QStringList extensions;

    // 未初期化メンバを作らない（docs/cpp-conventions.md §1）。
    bool canDecode = false;
    bool canEncode = false;

    // supportsAlpha / isLossless / supportsQuality は Qt に相当する API が無い。
    // buildFromQt() がメモリ上の往復で実測する（ADR-0007）。
    // 書き出せない形式は往復できないため、いずれも false に固定する。
    bool supportsAlpha = false;
    bool supportsQuality = false;
    bool isLossless = false;
};

class CapabilityTable {
public:
    // QImageReader / QImageWriter から実行時に構築する。
    [[nodiscard]] static CapabilityTable buildFromQt();

    // テスト用。任意の能力表を組み立てる。これがあるため convert() を
    // テンプレート化する必要はない（docs/cpp-conventions.md §2.3）。
    //
    // buildFromQt() と同じく id を正規化し、同一 FormatId は 1 件へ統合する。
    // 「1 つの FormatId に対して項目はちょうど 1 件」という不変条件を
    // どちらの構築経路でも保つため。
    [[nodiscard]] static CapabilityTable
    fromCapabilities(std::vector<FormatCapability> capabilities);

    // 引数を値でなく const 参照で受けるのは clang-tidy の
    // performance-unnecessary-value-param による（FormatId は QString を持つ）。
    [[nodiscard]] std::optional<FormatCapability> find(const FormatId& format) const noexcept;

    // canEncode な項目のみを id 昇順で返す。順序を固定するのは、
    // 呼び出しごとに並びが変わると UI と決定性テストが揺れるため。
    [[nodiscard]] std::vector<FormatCapability> encodable() const;

private:
    explicit CapabilityTable(std::vector<FormatCapability> capabilities) noexcept;

    std::vector<FormatCapability> capabilities_;
};

} // namespace katachi::core
