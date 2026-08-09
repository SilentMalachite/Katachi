#pragma once

// core 層の concept を集約するファイル。
//
// docs/cpp-conventions.md §2.6: 定義は層ごとに 1 ファイル。各ヘッダに散らさない。
// 層をまたぐ concept を作らない。
// ByteSource / ByteSink / ProgressSink は IoError を参照するため src/io/IoConcepts.hpp に置く
// （core に置くと依存方向 core → io → app が逆流する）。

#include <concepts>
#include <type_traits>

namespace katachi::core {

// Result の値側。
// nothrow move 構築を要求するのは Result::ok() が noexcept であるため
// （docs/cpp-conventions.md §2.2）。
template <typename T>
concept ResultValue = std::destructible<T> && std::is_nothrow_move_constructible_v<T>;

// Result のエラー側。
// equality_comparable を要求するのはテストで比較するため
// （docs/cpp-conventions.md §2.2）。
template <typename E>
concept ResultError =
    std::destructible<E> && std::copy_constructible<E> && std::equality_comparable<E>;

} // namespace katachi::core
