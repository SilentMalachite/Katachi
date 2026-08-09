// SettingsPanel のテスト（Phase 2 T8）。
//
// **選択肢はすべて能力表から作る。** フォーマット名の文字列リテラルは書かない
// （docs/spec-core.md §3 / 不変条件 INV3B）。テスト側も、特定の形式名を
// 直接書かずに能力表から選ぶ。環境で対応形式が変わっても意味を保つため。
//
// テスト名は ASCII に限る（Phase 0 の知見）。
#include "app/SettingsPanel.hpp"
#include "core/CapabilityTable.hpp"
#include "core/ConversionSpec.hpp"
#include "core/FormatId.hpp"
#include "io/CollisionPolicy.hpp"

#include <QList>
#include <QSize>
#include <QString>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <optional>

namespace {

using katachi::app::SettingsPanel;
using katachi::core::AlphaPolicy;
using katachi::core::CapabilityTable;
using katachi::core::FormatCapability;
using katachi::core::FormatId;
using katachi::core::IccPolicy;
using katachi::core::MetadataPolicy;
using katachi::io::CollisionPolicy;

const CapabilityTable& qtTable() {
    static const CapabilityTable table = CapabilityTable::buildFromQt();
    return table;
}

// 条件に合う形式を能力表から選ぶ。形式名を書かずに済ませるため。
std::optional<FormatCapability> findEncodable(bool wantsQuality) {
    for (const FormatCapability& capability : qtTable().encodable()) {
        if (capability.supportsQuality == wantsQuality) {
            return capability;
        }
    }
    return std::nullopt;
}

std::optional<FormatCapability> findWithAliases() {
    for (const FormatCapability& capability : qtTable().encodable()) {
        if (capability.extensions.size() > 1) {
            return capability;
        }
    }
    return std::nullopt;
}

} // namespace

TEST_CASE("the format list comes from the capability table", "[app][settings]") {
    const SettingsPanel panel(qtTable());

    const QList<FormatId> formats = panel.availableFormats();

    REQUIRE(formats.size() == static_cast<qsizetype>(qtTable().encodable().size()));
    REQUIRE_FALSE(formats.isEmpty());

    // 一覧のどれもが能力表で書き出し可能であること。
    for (const FormatId& format : formats) {
        const auto capability = qtTable().find(format);
        REQUIRE(capability.has_value());
        REQUIRE(capability->canEncode);
    }
}

TEST_CASE("the quality control follows supportsQuality", "[app][settings]") {
    const auto lossy = findEncodable(true);
    const auto plain = findEncodable(false);
    REQUIRE(lossy.has_value());
    REQUIRE(plain.has_value());

    SettingsPanel panel(qtTable());

    panel.selectFormat(lossy->id);
    REQUIRE(panel.isQualityEnabled());

    panel.selectFormat(plain->id);
    REQUIRE_FALSE(panel.isQualityEnabled());
}

TEST_CASE("the extension list comes from the selected format", "[app][settings]") {
    // 別名を持つ形式では拡張子を選べる（利用者の判断で .jpg を選べるようにするため）。
    const auto aliased = findWithAliases();
    REQUIRE(aliased.has_value());

    SettingsPanel panel(qtTable());
    panel.selectFormat(aliased->id);

    REQUIRE(panel.availableExtensions() == aliased->extensions);
    REQUIRE(panel.availableExtensions().size() > 1);

    // 既定は代表名。選ばなければ従来どおりの名前になる。
    REQUIRE(panel.extension().v == katachi::core::formatIdToString(aliased->id));

    // 別名を選べる。
    const QString alias =
        *std::ranges::find_if(aliased->extensions, [&aliased](const QString& value) {
            return value != formatIdToString(aliased->id);
        });
    panel.selectExtension(alias);
    REQUIRE(panel.extension().v == alias);
}

TEST_CASE("changing the format repopulates the extensions", "[app][settings]") {
    const auto aliased = findWithAliases();
    const auto plain = findEncodable(false);
    REQUIRE(aliased.has_value());
    REQUIRE(plain.has_value());

    SettingsPanel panel(qtTable());
    panel.selectFormat(aliased->id);
    panel.selectExtension(aliased->extensions.last());

    panel.selectFormat(plain->id);

    // 前の形式の拡張子が残っていないこと。
    REQUIRE(panel.availableExtensions() == plain->extensions);
    REQUIRE(panel.extension().v == katachi::core::formatIdToString(plain->id));
}

TEST_CASE("the panel produces the spec it displays", "[app][settings]") {
    const auto lossy = findEncodable(true);
    REQUIRE(lossy.has_value());

    SettingsPanel panel(qtTable());
    panel.selectFormat(lossy->id);
    panel.setQuality(42);
    panel.setResizeBound(QSize(320, 240));
    panel.setAlphaPolicy(AlphaPolicy::Flatten);
    panel.setMetadataPolicy(MetadataPolicy::StripAll);
    panel.setIccPolicy(IccPolicy::Strip);

    const auto spec = panel.spec();

    REQUIRE(spec.target == lossy->id);
    REQUIRE(spec.quality == 42);
    REQUIRE(spec.resize.has_value());
    REQUIRE(*spec.resize == QSize(320, 240));
    REQUIRE(spec.alpha == AlphaPolicy::Flatten);
    REQUIRE(spec.metadata == MetadataPolicy::StripAll);
    REQUIRE(spec.icc == IccPolicy::Strip);

    // リサイズは外せる。
    panel.setResizeBound(std::nullopt);
    REQUIRE_FALSE(panel.spec().resize.has_value());
}

TEST_CASE("the default collision policy shown is Skip", "[app][settings]") {
    // ADR-0005 / ADR-0009: 破壊的操作を既定にしない。
    const SettingsPanel panel(qtTable());

    REQUIRE(panel.collisionPolicy() == CollisionPolicy::Skip);
    REQUIRE(panel.collisionPolicy() != CollisionPolicy::Overwrite);
}

TEST_CASE("the default naming pattern keeps the source name", "[app][settings]") {
    const SettingsPanel panel(qtTable());

    // 既定は {name}.{ext}。入力名をそのまま使い、拡張子だけ差し替える。
    REQUIRE(panel.namePattern().v == QStringLiteral("{name}.{ext}"));
}
