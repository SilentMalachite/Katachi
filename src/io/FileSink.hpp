#pragma once

// ファイルへバイト列を書く（docs/spec-core.md §1）。`ByteSink` の本番実装。
//
// **`QSaveFile` を使う。** 一時ファイルへ書いてから改名するため、
// 途中で失敗しても部分的に書かれたファイルや壊れた出力を残さない。
//
// **衝突ポリシーはここで適用する**（ADR-0009）。実在確認はワーカースレッドで、
// そのファイルを書く直前に行う。事前に main thread で全件確認しない。

#include "core/Result.hpp"
#include "io/CollisionPolicy.hpp"
#include "io/IoError.hpp"

#include <QByteArray>
#include <QString>

#include <variant>

namespace katachi::io {

class FileSink {
public:
    FileSink(OutputDirectory directory, OutputFileName fileName,
             CollisionPolicy policy = defaultCollisionPolicy) noexcept;

    [[nodiscard]] core::Result<std::monostate, IoError> write(const QByteArray& bytes);

    // 衝突解決後に実際に書いたパス。**write() が成功したときのみ意味を持つ。**
    //
    // ByteSink concept には含めない。concept が要求する操作は原則 4 個以下であり
    // （docs/cpp-conventions.md §2.6）、この値が要るのは具象型を知っている
    // 呼び出し側（JobRunnerBridge）だけのため。
    [[nodiscard]] const QString& resolvedPath() const noexcept;

private:
    OutputDirectory directory_;
    OutputFileName fileName_;
    CollisionPolicy policy_;
    QString resolvedPath_;
};

} // namespace katachi::io
