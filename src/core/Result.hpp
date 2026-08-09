#pragma once

// 成功値と失敗値のどちらか一方を持つ型。
// C++23 の std::expected は使えないため自前で持つ（CLAUDE.md）。

#include "core/Concepts.hpp"

#include <cassert>
#include <concepts>
#include <utility>
#include <variant>

namespace katachi::core {

// 型引数は必ず制約する（docs/cpp-conventions.md §2）。
// !std::same_as<T,E> は飾りではない。Result<QString, QString> は構築が曖昧になり、
// 成功と失敗を取り違える。型で塞ぐ（docs/spec-core.md §2）。
template <ResultValue T, ResultError E>
    requires(!std::same_as<T, E>)
class Result {
public:
    [[nodiscard]] static Result ok(T value) noexcept { return Result(std::move(value)); }

    [[nodiscard]] static Result err(E error) noexcept { return Result(std::move(error)); }

    [[nodiscard]] bool isOk() const noexcept { return std::holds_alternative<T>(data_); }

    // !isOk() での呼び出しは契約違反。
    // 契約はテストではなく実行時アサートで担保する（docs/cpp-conventions.md §2.5）。
    // std::get は契約違反時に例外を投げるため使わない。core では例外を送出しない。
    [[nodiscard]] const T& value() const& {
        assert(isOk());
        return *std::get_if<T>(&data_);
    }

    [[nodiscard]] const E& error() const& {
        assert(!isOk());
        return *std::get_if<E>(&data_);
    }

private:
    explicit Result(T value) noexcept : data_(std::move(value)) {}
    explicit Result(E error) noexcept : data_(std::move(error)) {}

    std::variant<T, E> data_;
};

} // namespace katachi::core
