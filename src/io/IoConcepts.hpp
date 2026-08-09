#pragma once

// io 層の concept（docs/cpp-conventions.md §2.2）。
//
// **concept は層ごとにファイルを分ける。層をまたぐ concept を作らない**（同 §2.6）。
// core 層の concept は src/core/Concepts.hpp にある。
// ByteSource / ByteSink は IoError を参照するため、core に置くと依存方向が逆流する。

#include "core/Result.hpp"
#include "io/IoError.hpp"

#include <QByteArray>

#include <concepts>
#include <variant>

namespace katachi::io {

template <typename T>
concept ByteSource = requires(T& t) {
    { t.read() } -> std::same_as<core::Result<QByteArray, IoError>>;
};

template <typename T>
concept ByteSink = requires(T& t, const QByteArray& bytes) {
    { t.write(bytes) } -> std::same_as<core::Result<std::monostate, IoError>>;
};

// 進捗とキャンセル。Qt に依存せず JobRunner をテストするための抽象。
//
// **onProgress に noexcept を要求しない**（docs/cpp-conventions.md §2.4）。
// 本番の JobRunnerBridge はここで Qt のシグナルを emit するが、
// emit は接続先スロットを同期呼び出しするため noexcept にできない。
// 要求すると本番型が concept を満たせなくなる。
// isCancelled() は単なるフラグ読み出しなので noexcept を課す。
template <typename T>
concept ProgressSink = requires(T& t, int done, int total) {
    { t.onProgress(done, total) };
    { t.isCancelled() } noexcept -> std::same_as<bool>;
};

} // namespace katachi::io
