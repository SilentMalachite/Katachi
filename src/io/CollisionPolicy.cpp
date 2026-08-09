#include "io/CollisionPolicy.hpp"

#include "core/Result.hpp"
#include "io/IoError.hpp"

#include <QChar>
#include <QDir>
#include <QFileInfo>
#include <QString>

namespace katachi::io {
namespace {

using ResolveResult = core::Result<QString, IoError>;

} // namespace

core::Result<QString, IoError> resolveCollision(const OutputDirectory& directory,
                                                const OutputFileName& fileName,
                                                CollisionPolicy policy, int maxRenameAttempts) {
    const QDir dir(directory.v);
    const QString desired = dir.filePath(fileName.v);

    // 上書きは実在を確かめる必要がない。
    if (policy == CollisionPolicy::Overwrite) {
        return ResolveResult::ok(desired);
    }

    if (!QFileInfo::exists(desired)) {
        return ResolveResult::ok(desired);
    }

    if (policy == CollisionPolicy::Skip) {
        return ResolveResult::err(IoError::DestinationExists);
    }

    // Rename: 末尾のドットで基底名と拡張子に分け、その間に _N を挟む。
    // 拡張子を落とすと出力形式が分からなくなるため、必ず維持する。
    const QFileInfo info(fileName.v);
    const QString base = info.completeBaseName();
    const QString suffix = info.suffix();

    for (int attempt = 1; attempt <= maxRenameAttempts; ++attempt) {
        QString candidate = base + QLatin1Char('_') + QString::number(attempt);
        if (!suffix.isEmpty()) {
            candidate += QLatin1Char('.');
            candidate += suffix;
        }

        const QString path = dir.filePath(candidate);
        if (!QFileInfo::exists(path)) {
            return ResolveResult::ok(path);
        }
    }

    return ResolveResult::err(IoError::WriteFailed);
}

} // namespace katachi::io
