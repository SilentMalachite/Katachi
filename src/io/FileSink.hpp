#pragma once

// ファイルへバイト列を書く（docs/spec-core.md §1）。`ByteSink` の本番実装。
//
// **`QSaveFile` を使う。** 一時ファイルへ書いてから改名するため、
// 途中で失敗しても部分的に書かれたファイルや壊れた出力を残さない。
//
// **衝突ポリシー（ADR-0009）の適用は T3 で足す。** ここでは渡されたパスへ書くだけ。

#include "core/Result.hpp"
#include "io/IoError.hpp"

#include <QByteArray>
#include <QString>

#include <variant>

namespace katachi::io {

class FileSink {
public:
    explicit FileSink(QString path) noexcept;

    [[nodiscard]] core::Result<std::monostate, IoError> write(const QByteArray& bytes) const;

private:
    QString path_;
};

} // namespace katachi::io
