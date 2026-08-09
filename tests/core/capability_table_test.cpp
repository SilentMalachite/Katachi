// CapabilityTable と CapabilitySource concept のテスト。
// テストコードはフォーマット名の文字列リテラル禁止の対象外（docs/spec-core.md §3）。
// テスト名は ASCII に限る。
#include "core/CapabilityTable.hpp"
#include "core/Concepts.hpp"
#include "core/FormatId.hpp"

#include <QByteArray>
#include <QImageReader>
#include <QImageWriter>
#include <QSet>
#include <QString>
#include <QStringList>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

using katachi::core::CapabilitySource;
using katachi::core::CapabilityTable;
using katachi::core::FormatCapability;
using katachi::core::FormatId;
using katachi::core::formatIdFromString;

// 契約の明文化（docs/cpp-conventions.md §2.3）。
static_assert(CapabilitySource<CapabilityTable>);
// 否定側。制約が緩すぎて何でも通る事故を検出する（docs/phases.md §2.2）。
static_assert(!CapabilitySource<int>);
static_assert(!CapabilitySource<FormatId>);

namespace {

FormatCapability makeCapability(const QString& name, bool canEncode) {
    FormatCapability capability;
    capability.id = formatIdFromString(name);
    capability.extensions = QStringList{name};
    capability.canDecode = true;
    capability.canEncode = canEncode;
    return capability;
}

} // namespace

TEST_CASE("buildFromQt reports a non empty encodable set including PNG", "[core][capability]") {
    const CapabilityTable table = CapabilityTable::buildFromQt();

    REQUIRE_FALSE(table.encodable().empty());
    const auto png = table.find(formatIdFromString(QStringLiteral("png")));
    REQUIRE(png.has_value());
    REQUIRE(png->canEncode);
}

TEST_CASE("buildFromQt yields exactly one entry per FormatId", "[core][capability]") {
    const CapabilityTable table = CapabilityTable::buildFromQt();

    QSet<QString> seen;
    for (const FormatCapability& capability : table.encodable()) {
        REQUIRE_FALSE(seen.contains(capability.id.v));
        seen.insert(capability.id.v);
    }

    // Qt が報告する全名称を正規化しても、行き先は必ず表に 1 件だけ存在する。
    for (const QByteArray& name : QImageReader::supportedImageFormats()) {
        const auto found = table.find(formatIdFromString(QString::fromUtf8(name)));
        REQUIRE(found.has_value());
        REQUIRE(found->id == formatIdFromString(QString::fromUtf8(name)));
    }
}

TEST_CASE("buildFromQt merges jpeg aliases into a single entry", "[core][capability]") {
    const CapabilityTable table = CapabilityTable::buildFromQt();

    const auto viaJpeg = table.find(formatIdFromString(QStringLiteral("jpeg")));
    const auto viaJpg = table.find(formatIdFromString(QStringLiteral("jpg")));
    const auto viaJfif = table.find(formatIdFromString(QStringLiteral("jfif")));

    REQUIRE(viaJpeg.has_value());
    REQUIRE(viaJpg.has_value());
    REQUIRE(viaJfif.has_value());
    // 同じ 1 件を指していること。
    REQUIRE(viaJpeg->id == viaJpg->id);
    REQUIRE(viaJpeg->id == viaJfif->id);
    // extensions は和集合。Qt が報告した別名がすべて残る。
    REQUIRE(viaJpeg->extensions.contains(QStringLiteral("jpeg")));
    REQUIRE(viaJpeg->extensions.contains(QStringLiteral("jpg")));
}

TEST_CASE("buildFromQt classifies PNG as lossless with alpha", "[core][capability]") {
    const CapabilityTable table = CapabilityTable::buildFromQt();
    const auto png = table.find(formatIdFromString(QStringLiteral("png")));

    REQUIRE(png.has_value());
    REQUIRE(png->supportsAlpha);
    REQUIRE(png->isLossless);
}

TEST_CASE("buildFromQt classifies JPEG as lossy without alpha", "[core][capability]") {
    const CapabilityTable table = CapabilityTable::buildFromQt();
    const auto jpeg = table.find(formatIdFromString(QStringLiteral("jpeg")));

    REQUIRE(jpeg.has_value());
    REQUIRE_FALSE(jpeg->supportsAlpha);
    REQUIRE_FALSE(jpeg->isLossless);
    REQUIRE(jpeg->supportsQuality);
}

TEST_CASE("buildFromQt leaves unprobeable fields false for read only formats",
          "[core][capability]") {
    // 書き出せない形式はメモリ上で往復できないため判定不能。false で固定する（ADR-0007）。
    // 読み込み専用の集合は Qt から直接求める。どの形式が読み込み専用かは環境で変わるため。
    const CapabilityTable table = CapabilityTable::buildFromQt();

    QSet<QString> writable;
    for (const QByteArray& name : QImageWriter::supportedImageFormats()) {
        writable.insert(formatIdFromString(QString::fromUtf8(name)).v);
    }

    for (const QByteArray& name : QImageReader::supportedImageFormats()) {
        const FormatId id = formatIdFromString(QString::fromUtf8(name));
        if (writable.contains(id.v)) {
            continue;
        }
        const auto capability = table.find(id);
        REQUIRE(capability.has_value());
        REQUIRE_FALSE(capability->canEncode);
        REQUIRE_FALSE(capability->supportsAlpha);
        REQUIRE_FALSE(capability->isLossless);
        REQUIRE_FALSE(capability->supportsQuality);
    }
}

TEST_CASE("find returns nullopt for an unknown format", "[core][capability]") {
    const CapabilityTable table =
        CapabilityTable::fromCapabilities({makeCapability(QStringLiteral("png"), true)});

    REQUIRE(table.find(formatIdFromString(QStringLiteral("png"))).has_value());
    REQUIRE_FALSE(table.find(formatIdFromString(QStringLiteral("nosuchformat"))).has_value());
}

TEST_CASE("fromCapabilities merges entries that normalize to the same id", "[core][capability]") {
    const CapabilityTable table = CapabilityTable::fromCapabilities(
        {makeCapability(QStringLiteral("jpg"), false), makeCapability(QStringLiteral("JPEG"), true),
         makeCapability(QStringLiteral("jfif"), false)});

    const auto jpeg = table.find(formatIdFromString(QStringLiteral("jpeg")));

    REQUIRE(jpeg.has_value());
    // 3 件が 1 件へ統合され、encodable() にも 1 件しか現れない。
    REQUIRE(table.encodable().size() == 1);
    // 真偽値は論理和、extensions は和集合。
    REQUIRE(jpeg->canEncode);
    REQUIRE(jpeg->extensions.contains(QStringLiteral("jpg")));
    REQUIRE(jpeg->extensions.contains(QStringLiteral("jfif")));
    // 別名のどれで引いても同じ 1 件に届く。
    REQUIRE(table.find(formatIdFromString(QStringLiteral("jpg")))->id == jpeg->id);
    REQUIRE(table.find(formatIdFromString(QStringLiteral("jfif")))->id == jpeg->id);
}

TEST_CASE("encodable returns only entries that can encode", "[core][capability]") {
    const CapabilityTable table =
        CapabilityTable::fromCapabilities({makeCapability(QStringLiteral("png"), true),
                                           makeCapability(QStringLiteral("gif"), false)});

    const std::vector<FormatCapability> encodable = table.encodable();

    REQUIRE(encodable.size() == 1);
    REQUIRE(encodable.front().id == formatIdFromString(QStringLiteral("png")));
}

TEST_CASE("encodable is ordered deterministically", "[core][capability]") {
    const CapabilityTable table = CapabilityTable::buildFromQt();

    const std::vector<FormatCapability> first = table.encodable();
    const std::vector<FormatCapability> second = table.encodable();

    REQUIRE(first.size() == second.size());
    REQUIRE(std::is_sorted(first.begin(), first.end(),
                           [](const FormatCapability& lhs, const FormatCapability& rhs) {
                               return lhs.id.v < rhs.id.v;
                           }));
}
