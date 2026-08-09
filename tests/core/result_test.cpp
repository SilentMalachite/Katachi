// Result<T,E> と core 層 concept のテスト。
// テストコードはフォーマット名の文字列リテラル禁止・concept 制約の対象外
// （docs/spec-core.md §3、走査対象は src/ のみ）。
//
// テスト名は ASCII に限る（Windows のコンソール encoding で化けるため）。
#include "core/Concepts.hpp"
#include "core/Result.hpp"

#include <QByteArray>
#include <QString>

#include <catch2/catch_test_macros.hpp>

#include <utility>

namespace {

enum class TestError { Alpha, Beta };

// ResultValue の否定側。move 構築が noexcept でないため ok() の noexcept を支えられない。
struct ThrowingMove {
    ThrowingMove() = default;
    ThrowingMove(const ThrowingMove&) = default;
    ThrowingMove(ThrowingMove&&) noexcept(false) {}
    ThrowingMove& operator=(const ThrowingMove&) = default;
    ThrowingMove& operator=(ThrowingMove&&) noexcept(false) { return *this; }
    ~ThrowingMove() = default;
};

// ResultError の否定側。operator== を持たないためテストで比較できない。
struct NoEquality {
    int v = 0;
};

} // namespace

using katachi::core::Result;
using katachi::core::ResultError;
using katachi::core::ResultValue;

// --- concept の肯定側 ---
static_assert(ResultValue<int>);
static_assert(ResultValue<QByteArray>);
static_assert(ResultValue<QString>);
static_assert(ResultError<TestError>);

// --- concept の否定側 ---
// docs/phases.md §2.2: 制約が緩すぎて何でも通る事故を検出するため、
// 適合しない型への static_assert(!C<U>) を必ず 1 つ以上置く。
static_assert(!ResultValue<ThrowingMove>);
static_assert(!ResultError<NoEquality>);

// --- T == E の排除（docs/spec-core.md §2）---
// Result<QString, QString> は両 concept を満たすが requires(!same_as<T,E>) で弾かれる。
//
// 否定側を static_assert(!requires { typename Result<QString, QString>; }) では書けない。
// 制約を満たさないクラステンプレートの特殊化を名前で指すことは代入失敗ではなく
// ハードエラーになり、requires 式が false になる前にコンパイルが止まるため。
// 代わりに「ビルドが失敗すること」を ctest で確認する
//   -> テスト core.result.rejects_same_type / tests/core/compile_fail/result_same_type.cpp
//
// 肯定側（T != E なら特殊化できること）はここで確認する。
static_assert(requires { typename Result<QString, TestError>; });

// QString が T としても E としても concept を満たすことを示す。
// これにより、上のコンパイル失敗テストが落ちる理由が
// 「concept 不適合」ではなく「requires(!same_as<T,E>)」であることが確定する。
static_assert(ResultValue<QString> && ResultError<QString>);

TEST_CASE("Result::ok reports success and yields the value", "[core][result]") {
    const auto result = Result<int, TestError>::ok(42);

    REQUIRE(result.isOk());
    REQUIRE(result.value() == 42);
}

TEST_CASE("Result::err reports failure and yields the error", "[core][result]") {
    const auto result = Result<int, TestError>::err(TestError::Alpha);

    REQUIRE_FALSE(result.isOk());
    REQUIRE(result.error() == TestError::Alpha);
}

TEST_CASE("Result carries a moved-in QByteArray payload", "[core][result]") {
    QByteArray payload(3, 'x');
    const auto result = Result<QByteArray, TestError>::ok(std::move(payload));

    REQUIRE(result.isOk());
    REQUIRE(result.value().size() == 3);
}

TEST_CASE("Result distinguishes error values of the same type", "[core][result]") {
    const auto alpha = Result<int, TestError>::err(TestError::Alpha);
    const auto beta = Result<int, TestError>::err(TestError::Beta);

    REQUIRE(alpha.error() == TestError::Alpha);
    REQUIRE(beta.error() == TestError::Beta);
    REQUIRE(alpha.error() != beta.error());
}
