#include "io/FileSink.hpp"

#include "core/Result.hpp"
#include "io/CollisionPolicy.hpp"
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

FileSink::FileSink(OutputDirectory directory, OutputFileName fileName,
                   CollisionPolicy policy) noexcept
    : directory_(std::move(directory)), fileName_(std::move(fileName)), policy_(policy) {}

core::Result<std::monostate, IoError> FileSink::write(const QByteArray& bytes) {
    // 衝突の解決は書く直前（ADR-0009）。事前に決めると、その間に外で
    // ファイルが増えたときに取りこぼす。
    const core::Result<QString, IoError> resolved =
        resolveCollision(directory_, fileName_, policy_);
    if (!resolved.isOk()) {
        // Skip による DestinationExists もここを通る。失敗ではないが、
        // 「書かなかった」ことは呼び出し側へ伝える必要がある。
        return WriteResult::err(resolved.error());
    }

    QSaveFile file(resolved.value());

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

    resolvedPath_ = resolved.value();

    return WriteResult::ok(std::monostate{});
}

const QString& FileSink::resolvedPath() const noexcept { return resolvedPath_; }

} // namespace katachi::io
