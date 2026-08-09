#pragma once

// core 層の concept を集約するファイル。
//
// docs/cpp-conventions.md §2.6: 定義は層ごとに 1 ファイル。各ヘッダに散らさない。
// 層をまたぐ concept を作らない。
// ByteSource / ByteSink / ProgressSink は IoError を参照するため src/io/IoConcepts.hpp に置く
// （core に置くと依存方向 core → io → app が逆流する）。

#include "core/CapabilityTable.hpp"
#include "core/FormatId.hpp"

#include <concepts>
#include <optional>
#include <type_traits>
#include <vector>

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

// 能力表の抽象（docs/cpp-conventions.md §2.2）。
// 用途は static_assert による契約の明文化に留める（同 §2.3）。
// convert() は具象 CapabilityTable を受け取り、テンプレート化しない。
// 差し替えたいのは能力表の「中身」であって型ではなく、fromCapabilities() で足りるため。
template <typename T>
concept CapabilitySource = requires(const T& source, FormatId format) {
    { source.find(format) } -> std::same_as<std::optional<FormatCapability>>;
    { source.encodable() } -> std::same_as<std::vector<FormatCapability>>;
};

} // namespace katachi::core
