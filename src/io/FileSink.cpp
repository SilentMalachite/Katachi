#include "io/FileSink.hpp"

#include "core/Result.hpp"
#include "io/IoConcepts.hpp"
#include "io/IoError.hpp"

#include <QByteArray>
#include <QIODevice>
#include <QSaveFile>
#include <QString>

#include <utility>
#include <variant>

namespace katachi::io {
namespace {

using WriteResult = core::Result<std::monostate, IoError>;

} // namespace

// 契約の明文化（docs/phases.md §2.2 が求める本番型の static_assert）。
static_assert(ByteSink<FileSink>);

FileSink::FileSink(QString path) noexcept : path_(std::move(path)) {}

core::Result<std::monostate, IoError> FileSink::write(const QByteArray& bytes) const {
    QSaveFile file(path_);

    // 親ディレクトリが無い場合はここで落ちる。ディレクトリは自動で作らない
    // （利用者が指していないところへ書かない）。
    if (!file.open(QIODevice::WriteOnly)) {
        return WriteResult::err(IoError::WriteFailed);
    }

    if (file.write(bytes) != bytes.size()) {
        // commit しなければ一時ファイルは捨てられるが、意図を明示しておく。
        file.cancelWriting();
        return WriteResult::err(IoError::WriteFailed);
    }

    // commit() で初めて目的のパスへ現れる。ここまでは一時ファイルのまま。
    if (!file.commit()) {
        return WriteResult::err(IoError::WriteFailed);
    }

    return WriteResult::ok(std::monostate{});
}

} // namespace katachi::io
