#pragma once

// ファイルからバイト列を読む（docs/spec-core.md §1）。`ByteSource` の本番実装。
//
// 上限を持つのは ADR-0008 による。**読み込むだけで予算を使い切る入力は、
// 復号すればまず破綻する。だから読む前にサイズで落とす。**

#include "core/Result.hpp"
#include "io/IoError.hpp"

#include <QByteArray>
#include <QString>
#include <QtTypes>

namespace katachi::io {

// 既定の上限は ADR-0008 の予算総量と同じ 1 GiB。
// バッチ全体の予算を 1 件で使い切る入力は、そもそも読ませない。
inline constexpr qint64 defaultMaxReadBytes = 1LL << 30;

class FileSource {
public:
    explicit FileSource(QString path, qint64 maxBytes = defaultMaxReadBytes) noexcept;

    [[nodiscard]] core::Result<QByteArray, IoError> read() const;

private:
    QString path_;
    qint64 maxBytes_;
};

} // namespace katachi::io
