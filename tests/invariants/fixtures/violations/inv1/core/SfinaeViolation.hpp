// INV1 の検出確認用フィクスチャ。故意に違反している。ビルド対象ではない。
#pragma once

#include <type_traits>

template <typename T>
    requires std::is_integral_v<T>
struct Sfinae {
    using type = std::enable_if_t<std::is_integral_v<T>, T>;
};
