#pragma once

// io 層のエラー（docs/spec-core.md §1）。**core には置かない。**
//
// 列挙値は「実際に発生させられるもの」だけを定義する。
// docs/phases.md §2.2 は全列挙値にテストがあることを要求しており、
// 起こせない値を作るとその要求を満たせなくなるため。

#include <cstdint>

namespace katachi::io {

// 基底型を明示するのは clang-tidy の performance-enum-size による（Phase 1 T2 の知見）。
enum class IoError : std::uint8_t {
    NotFound,    // 入力のパスが存在しない
    OpenFailed,  // 開けない（ディレクトリを指している等）
    WriteFailed, // 書き出せない（親ディレクトリが無い、改名候補を使い切った — ADR-0009）
    TooLarge,    // 入力が上限を超える（ADR-0008）
    // 宛先が既にあり CollisionPolicy::Skip のため書かなかった（ADR-0009）。
    // 失敗ではない。app 層は「スキップ（既存）」と表示し、失敗件数に数えない。
    DestinationExists,
};

} // namespace katachi::io
