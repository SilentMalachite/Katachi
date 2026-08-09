// resolveOutputName() のテスト。
//
// 期待値は docs/spec-core.md §5 の例から導いている。
//   "{name}_{index:03}.{ext}" -> "photo_001.png"
// テスト名は ASCII に限る。
#include "core/Concepts.hpp"
#include "core/NamingRule.hpp"

#include <QString>

#include <catch2/catch_test_macros.hpp>

using katachi::core::NameExtension;
using katachi::core::NamePattern;
using katachi::core::NamingError;
using katachi::core::resolveOutputName;
using katachi::core::ResultError;

// NamingError は Result のエラー側に載る。
static_assert(ResultError<NamingError>);

namespace {

// 呼び出しを短く保つための小さな包み。強い型の効果（取り違え防止）は
// 本番の呼び出し側で効くので、テスト内での簡略化は問題にならない。
auto resolve(const QString& base, int index, const QString& pattern, const QString& extension) {
    return resolveOutputName(base, index, NamePattern{pattern}, NameExtension{extension});
}

} // namespace

TEST_CASE("resolveOutputName matches the documented example", "[core][naming]") {
    // docs/spec-core.md §5 の例そのもの。
    const auto result = resolve(QStringLiteral("photo"), 1,
                                QStringLiteral("{name}_{index:03}.{ext}"), QStringLiteral("png"));

    REQUIRE(result.isOk());
    REQUIRE(result.value() == QStringLiteral("photo_001.png"));
}

TEST_CASE("index without a width is not padded", "[core][naming]") {
    const auto result =
        resolve(QStringLiteral("a"), 42, QStringLiteral("{index}"), QStringLiteral("png"));

    REQUIRE(result.isOk());
    REQUIRE(result.value() == QStringLiteral("42"));
}

TEST_CASE("index width pads with zeros", "[core][naming]") {
    const auto result =
        resolve(QStringLiteral("a"), 42, QStringLiteral("{index:5}"), QStringLiteral("png"));

    REQUIRE(result.isOk());
    REQUIRE(result.value() == QStringLiteral("00042"));
}

TEST_CASE("index wider than the width is not truncated", "[core][naming]") {
    const auto result =
        resolve(QStringLiteral("a"), 123456, QStringLiteral("{index:3}"), QStringLiteral("png"));

    REQUIRE(result.isOk());
    REQUIRE(result.value() == QStringLiteral("123456"));
}

TEST_CASE("a negative index keeps its sign before the padding", "[core][naming]") {
    const auto result =
        resolve(QStringLiteral("a"), -7, QStringLiteral("{index:3}"), QStringLiteral("png"));

    REQUIRE(result.isOk());
    REQUIRE(result.value() == QStringLiteral("-007"));
}

TEST_CASE("literal text around placeholders is preserved", "[core][naming]") {
    const auto result =
        resolve(QStringLiteral("photo"), 2, QStringLiteral("out/{name}-v{index}.{ext}"),
                QStringLiteral("webp"));

    REQUIRE(result.isOk());
    REQUIRE(result.value() == QStringLiteral("out/photo-v2.webp"));
}

TEST_CASE("a placeholder may appear more than once", "[core][naming]") {
    const auto result =
        resolve(QStringLiteral("x"), 1, QStringLiteral("{name}{name}"), QStringLiteral("png"));

    REQUIRE(result.isOk());
    REQUIRE(result.value() == QStringLiteral("xx"));
}

TEST_CASE("resolveOutputName is deterministic", "[core][naming]") {
    const auto first = resolve(QStringLiteral("photo"), 7,
                               QStringLiteral("{name}_{index:04}.{ext}"), QStringLiteral("tiff"));
    const auto second = resolve(QStringLiteral("photo"), 7,
                                QStringLiteral("{name}_{index:04}.{ext}"), QStringLiteral("tiff"));

    REQUIRE(first.isOk());
    REQUIRE(second.isOk());
    REQUIRE(first.value() == second.value());
}

// -------------------------------------------------------------- エラー網羅
// docs/phases.md §2.2 と同じ方針で、NamingError の全 4 列挙値に発生テストを置く。

TEST_CASE("an empty pattern is rejected", "[core][naming][error]") {
    const auto result = resolve(QStringLiteral("photo"), 1, QString{}, QStringLiteral("png"));

    REQUIRE_FALSE(result.isOk());
    REQUIRE(result.error() == NamingError::EmptyPattern);
}

TEST_CASE("an unknown placeholder is rejected", "[core][naming][error]") {
    const auto result =
        resolve(QStringLiteral("photo"), 1, QStringLiteral("{nope}"), QStringLiteral("png"));

    REQUIRE_FALSE(result.isOk());
    REQUIRE(result.error() == NamingError::UnknownPlaceholder);
}

TEST_CASE("an unterminated placeholder is rejected", "[core][naming][error]") {
    // 閉じ括弧が無いものは黙って literal 扱いにしない。打ち間違いを見逃さないため。
    const auto result =
        resolve(QStringLiteral("photo"), 1, QStringLiteral("{name"), QStringLiteral("png"));

    REQUIRE_FALSE(result.isOk());
    REQUIRE(result.error() == NamingError::UnknownPlaceholder);
}

TEST_CASE("an index spec that is not digits is rejected", "[core][naming][error]") {
    for (const QString& pattern : {QStringLiteral("{index:}"), QStringLiteral("{index:ab}"),
                                   QStringLiteral("{index:-3}"), QStringLiteral("{index:3x}")}) {
        INFO(pattern.toStdString());
        const auto result = resolve(QStringLiteral("photo"), 1, pattern, QStringLiteral("png"));

        REQUIRE_FALSE(result.isOk());
        REQUIRE(result.error() == NamingError::InvalidIndexSpec);
    }
}

TEST_CASE("an absurdly large index width is rejected", "[core][naming][error]") {
    // 上限を設けないと、巨大な桁指定で確保が走り std::terminate しうる（ADR-0002）。
    const auto result =
        resolve(QStringLiteral("photo"), 1, QStringLiteral("{index:99}"), QStringLiteral("png"));

    REQUIRE_FALSE(result.isOk());
    REQUIRE(result.error() == NamingError::InvalidIndexSpec);
}

TEST_CASE("an empty expansion is rejected", "[core][naming][error]") {
    const auto result = resolve(QString{}, 1, QStringLiteral("{name}"), QStringLiteral("png"));

    REQUIRE_FALSE(result.isOk());
    REQUIRE(result.error() == NamingError::EmptyResult);
}

TEST_CASE("a colon on a placeholder that takes no spec is rejected", "[core][naming][error]") {
    const auto result =
        resolve(QStringLiteral("photo"), 1, QStringLiteral("{name:03}"), QStringLiteral("png"));

    REQUIRE_FALSE(result.isOk());
    REQUIRE(result.error() == NamingError::UnknownPlaceholder);
}
