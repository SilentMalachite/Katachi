#include "io/FileSource.hpp"

#include "core/Result.hpp"
#include "io/IoConcepts.hpp"
#include "io/IoError.hpp"

#include <QByteArray>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QIODevice>
#include <QString>
#include <QtTypes>

#include <utility>

namespace katachi::io {
namespace {

using ReadResult = core::Result<QByteArray, IoError>;

} // namespace

// 契約の明文化（docs/phases.md §2.2 が求める本番型の static_assert）。
// ヘッダではなくここに置くのは、IoConcepts.hpp を利用者へ押し付けないため。
// src/core/CapabilityTable.cpp と同じ置き方。
static_assert(ByteSource<FileSource>);

FileSource::FileSource(QString path, qint64 maxBytes) noexcept
    : path_(std::move(path)), maxBytes_(maxBytes) {}

core::Result<QByteArray, IoError> FileSource::read() const {
    const QFileInfo info(path_);

    if (!info.exists()) {
        return ReadResult::err(IoError::NotFound);
    }
    // ディレクトリや特殊ファイルはここで落とす。open() まで進めない。
    if (!info.isFile()) {
        return ReadResult::err(IoError::OpenFailed);
    }
    // ADR-0008: 上限の判定は読み込みより前。読んでから測っても手遅れになる。
    if (info.size() > maxBytes_) {
        return ReadResult::err(IoError::TooLarge);
    }

    QFile file(path_);
    if (!file.open(QIODevice::ReadOnly)) {
        return ReadResult::err(IoError::OpenFailed);
    }

    QByteArray bytes = file.readAll();

    // 開けたが読み切れなかった場合もここへ落とす。
    // **黙って短いバイト列を返さない。** 途中で切れた入力をそのまま変換すると、
    // 「壊れた画像」ではなく「壊れた出力」になって外へ出ていく。
    //
    // 専用の列挙値を作らないのは、移植可能な形で発生させる手段が無く、
    // docs/phases.md §2.2 の「全列挙値にテストがある」を満たせないため。
    if (file.error() != QFileDevice::NoError) {
        return ReadResult::err(IoError::OpenFailed);
    }

    return ReadResult::ok(std::move(bytes));
}

} // namespace katachi::io
