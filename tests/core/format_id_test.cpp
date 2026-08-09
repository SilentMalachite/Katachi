// FormatId と ConvertError / ConvertWarning のテスト。
// テストコードはフォーマット名の文字列リテラル禁止の対象外
// （docs/spec-core.md §3、スキャナの走査対象は src/ のみ）。
//
// テスト名は ASCII に限る（Windows のコンソール encoding で化けるため）。
#include "core/Concepts.hpp"
#include "core/ConvertError.hpp"
#include "core/FormatId.hpp"
#include "core/Result.hpp"

#include <QString>

#include <catch2/catch_test_macros.hpp>

using katachi::core::ConvertError;
using katachi::core::ConvertWarning;
using katachi::core::FormatId;
using katachi::core::formatIdFromString;
using katachi::core::formatIdToString;
using katachi::core::ResultError;
using katachi::core::ResultValue;

// FormatId は Result の値側に載る（CapabilityTable::find などで使う）。
static_assert(ResultValue<FormatId>);
// ConvertError は Result のエラー側に載る。
static_assert(ResultError<ConvertError>);

TEST_CASE("formatIdFromString round-trips through formatIdToString", "[core][formatid]") {
    const FormatId id = formatIdFromString(QStringLiteral("png"));

    REQUIRE(formatIdToString(id) == QStringLiteral("png"));
}

TEST_CASE("formatIdFromString folds case so lookups are stable", "[core][formatid]") {
    REQUIRE(formatIdFromString(QStringLiteral("PNG")) == formatIdFromString(QStringLiteral("png")));
    REQUIRE(formatIdFromString(QStringLiteral("JpEg")) ==
            formatIdFromString(QStringLiteral("jpeg")));
}

TEST_CASE("formatIdFromString trims surrounding whitespace", "[core][formatid]") {
    REQUIRE(formatIdFromString(QStringLiteral("  png\t")) ==
            formatIdFromString(QStringLiteral("png")));
}

TEST_CASE("formatIdFromString folds jpeg aliases onto one id", "[core][formatid]") {
    // Qt はいずれも MIME image/jpeg を報告する（ADR-0006）。
    const FormatId canonical = formatIdFromString(QStringLiteral("jpeg"));

    REQUIRE(formatIdFromString(QStringLiteral("jpg")) == canonical);
    REQUIRE(formatIdFromString(QStringLiteral("JPG")) == canonical);
    REQUIRE(formatIdFromString(QStringLiteral("jfif")) == canonical);
    REQUIRE(formatIdToString(canonical) == QStringLiteral("jpeg"));
}

TEST_CASE("formatIdFromString folds tiff aliases onto one id", "[core][formatid]") {
    const FormatId canonical = formatIdFromString(QStringLiteral("tiff"));

    REQUIRE(formatIdFromString(QStringLiteral("tif")) == canonical);
    REQUIRE(formatIdFromString(QStringLiteral("  TIF ")) == canonical);
    REQUIRE(formatIdToString(canonical) == QStringLiteral("tiff"));
}

TEST_CASE("formatIdFromString keeps heic and heif apart", "[core][formatid]") {
    // MIME が image/heic と image/heif で異なるため畳んではならない（ADR-0006）。
    REQUIRE_FALSE(formatIdFromString(QStringLiteral("heic")) ==
                  formatIdFromString(QStringLiteral("heif")));
}

TEST_CASE("formatIdFromString leaves unrelated names untouched", "[core][formatid]") {
    REQUIRE(formatIdToString(formatIdFromString(QStringLiteral("png"))) == QStringLiteral("png"));
    REQUIRE(formatIdToString(formatIdFromString(QStringLiteral("webp"))) == QStringLiteral("webp"));
    REQUIRE(formatIdToString(formatIdFromString(QStringLiteral("bmp"))) == QStringLiteral("bmp"));
}

TEST_CASE("FormatId distinguishes different names", "[core][formatid]") {
    REQUIRE_FALSE(formatIdFromString(QStringLiteral("png")) ==
                  formatIdFromString(QStringLiteral("bmp")));
}

TEST_CASE("FormatId comparison is value based", "[core][formatid]") {
    const FormatId a = formatIdFromString(QStringLiteral("webp"));
    const FormatId b = formatIdFromString(QStringLiteral("webp"));

    REQUIRE(a == b);
    REQUIRE(a.v == b.v);
}

TEST_CASE("ConvertError values are distinct and comparable", "[core][error]") {
    REQUIRE(ConvertError::EmptyInput != ConvertError::DecodeFailed);
    REQUIRE(ConvertError::UnsupportedTarget != ConvertError::EncodeFailed);
    REQUIRE(ConvertError::AlphaLossNotAllowed != ConvertError::ImageTooLarge);
    REQUIRE(ConvertError::EmptyInput == ConvertError::EmptyInput);
}

TEST_CASE("ConvertWarning carries the alpha fallback case", "[core][warning]") {
    // docs/spec-core.md §4 の 3 行目に対応する唯一の警告（ADR-0004）。
    const ConvertWarning warning = ConvertWarning::AlphaFlattenedFallback;

    REQUIRE(warning == ConvertWarning::AlphaFlattenedFallback);
}
