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

// 仮引数名が docs/cpp-conventions.md §2.2 の t ではないのは、clang-tidy の
// readability-identifier-length（3 文字未満を禁止）による。
// src/core/Concepts.hpp が同じ理由で source / format としているのに合わせた。
// **要求する操作は §2.2 のまま。契約は変えていない。**
template <typename T>
concept ByteSource = requires(T& source) {
    { source.read() } -> std::same_as<core::Result<QByteArray, IoError>>;
};

template <typename T>
concept ByteSink = requires(T& sink, const QByteArray& bytes) {
    { sink.write(bytes) } -> std::same_as<core::Result<std::monostate, IoError>>;
};

// 進捗とキャンセル。Qt に依存せず JobRunner をテストするための抽象。
//
// **onProgress に noexcept を要求しない**（docs/cpp-conventions.md §2.4）。
// 本番の JobRunnerBridge はここで Qt のシグナルを emit するが、
// emit は接続先スロットを同期呼び出しするため noexcept にできない。
// 要求すると本番型が concept を満たせなくなる。
// isCancelled() は単なるフラグ読み出しなので noexcept を課す。
template <typename T>
concept ProgressSink = requires(T& sink, int done, int total) {
    { sink.onProgress(done, total) };
    { sink.isCancelled() } noexcept -> std::same_as<bool>;
};

} // namespace katachi::io
