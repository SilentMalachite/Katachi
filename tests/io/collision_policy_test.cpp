// 衝突ポリシーのテスト（Phase 2 T3 / ADR-0009）。
//
// 既定は Skip。破壊的操作を既定にしない（ADR-0005 の明示的な指示）。
// **既定値を確認せずに Overwrite にしないこと。**
//
// テスト名は ASCII に限る（Phase 0 の知見）。
#include "core/Result.hpp"
#include "io/CollisionPolicy.hpp"
#include "io/FileSink.hpp"
#include "io/IoError.hpp"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

namespace {

using katachi::io::CollisionPolicy;
using katachi::io::defaultCollisionPolicy;
using katachi::io::defaultMaxRenameAttempts;
using katachi::io::FileSink;
using katachi::io::IoError;
using katachi::io::OutputDirectory;
using katachi::io::OutputFileName;
using katachi::io::resolveCollision;

constexpr auto allEntries = QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden;

void writeRawFile(const QDir& dir, const QString& name, const QByteArray& bytes) {
    QFile file(dir.filePath(name));
    REQUIRE(file.open(QIODevice::WriteOnly));
    REQUIRE(file.write(bytes) == bytes.size());
    file.close();
}

QByteArray readRawFile(const QDir& dir, const QString& name) {
    QFile file(dir.filePath(name));
    REQUIRE(file.open(QIODevice::ReadOnly));
    return file.readAll();
}

} // namespace

TEST_CASE("the default collision policy is Skip", "[io][collision]") {
    // ADR-0005 / ADR-0009: 破壊的操作を既定にしない。
    static_assert(defaultCollisionPolicy == CollisionPolicy::Skip);
    static_assert(defaultCollisionPolicy != CollisionPolicy::Overwrite);
    // 改名の上限。無限ループしないための歯止め（ADR-0009）。
    static_assert(defaultMaxRenameAttempts == 10000);

    // ポリシーを省略して構築した FileSink が、実際に上書きしないことまで見る。
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QDir dir(temp.path());
    writeRawFile(dir, QStringLiteral("out.bin"), QByteArray("old"));

    FileSink sink(OutputDirectory{temp.path()}, OutputFileName{QStringLiteral("out.bin")});
    const auto result = sink.write(QByteArray("new"));

    REQUIRE_FALSE(result.isOk());
    REQUIRE(result.error() == IoError::DestinationExists);
    REQUIRE(readRawFile(dir, QStringLiteral("out.bin")) == QByteArray("old"));
}

TEST_CASE("no collision keeps the requested name", "[io][collision]") {
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QDir dir(temp.path());

    FileSink sink(OutputDirectory{temp.path()}, OutputFileName{QStringLiteral("photo.png")},
                  CollisionPolicy::Rename);
    const auto result = sink.write(QByteArray("bytes"));

    REQUIRE(result.isOk());
    REQUIRE(sink.resolvedPath() == dir.filePath(QStringLiteral("photo.png")));
    REQUIRE(readRawFile(dir, QStringLiteral("photo.png")) == QByteArray("bytes"));
}

TEST_CASE("Overwrite replaces the existing file", "[io][collision]") {
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QDir dir(temp.path());
    writeRawFile(dir, QStringLiteral("photo.png"), QByteArray("old"));

    FileSink sink(OutputDirectory{temp.path()}, OutputFileName{QStringLiteral("photo.png")},
                  CollisionPolicy::Overwrite);
    const auto result = sink.write(QByteArray("new"));

    REQUIRE(result.isOk());
    REQUIRE(sink.resolvedPath() == dir.filePath(QStringLiteral("photo.png")));
    REQUIRE(readRawFile(dir, QStringLiteral("photo.png")) == QByteArray("new"));
    // 余計なファイルを作らない。
    REQUIRE(dir.entryList(allEntries).size() == 1);
}

TEST_CASE("Skip leaves the existing file untouched", "[io][collision]") {
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QDir dir(temp.path());
    writeRawFile(dir, QStringLiteral("photo.png"), QByteArray("old"));
    const QStringList before = dir.entryList(allEntries);

    FileSink sink(OutputDirectory{temp.path()}, OutputFileName{QStringLiteral("photo.png")},
                  CollisionPolicy::Skip);
    const auto result = sink.write(QByteArray("new"));

    REQUIRE_FALSE(result.isOk());
    REQUIRE(result.error() == IoError::DestinationExists);
    REQUIRE(readRawFile(dir, QStringLiteral("photo.png")) == QByteArray("old"));
    // 別名でこっそり書いていないこと。
    REQUIRE(dir.entryList(allEntries) == before);
}

TEST_CASE("Rename appends _1 when the name is taken", "[io][collision]") {
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QDir dir(temp.path());
    writeRawFile(dir, QStringLiteral("photo.png"), QByteArray("old"));

    FileSink sink(OutputDirectory{temp.path()}, OutputFileName{QStringLiteral("photo.png")},
                  CollisionPolicy::Rename);
    const auto result = sink.write(QByteArray("new"));

    REQUIRE(result.isOk());
    REQUIRE(sink.resolvedPath() == dir.filePath(QStringLiteral("photo_1.png")));
    REQUIRE(readRawFile(dir, QStringLiteral("photo_1.png")) == QByteArray("new"));
    // 元のファイルは触らない。拡張子も落とさない。
    REQUIRE(readRawFile(dir, QStringLiteral("photo.png")) == QByteArray("old"));
}

TEST_CASE("Rename continues to _2", "[io][collision]") {
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QDir dir(temp.path());
    writeRawFile(dir, QStringLiteral("photo.png"), QByteArray("old"));
    writeRawFile(dir, QStringLiteral("photo_1.png"), QByteArray("older"));

    FileSink sink(OutputDirectory{temp.path()}, OutputFileName{QStringLiteral("photo.png")},
                  CollisionPolicy::Rename);
    const auto result = sink.write(QByteArray("new"));

    REQUIRE(result.isOk());
    REQUIRE(sink.resolvedPath() == dir.filePath(QStringLiteral("photo_2.png")));
    REQUIRE(readRawFile(dir, QStringLiteral("photo_2.png")) == QByteArray("new"));
}

TEST_CASE("Rename gives up after the limit", "[io][collision]") {
    // 上限が無いと、名前が埋まった状況で無限ループする（ADR-0009）。
    // 既定の 10000 個をテストで作るのは CI で重すぎるため、上限を注入して同じ経路を通す。
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QDir dir(temp.path());
    writeRawFile(dir, QStringLiteral("photo.png"), QByteArray("a"));
    writeRawFile(dir, QStringLiteral("photo_1.png"), QByteArray("b"));
    writeRawFile(dir, QStringLiteral("photo_2.png"), QByteArray("c"));
    writeRawFile(dir, QStringLiteral("photo_3.png"), QByteArray("d"));

    const auto result =
        resolveCollision(OutputDirectory{temp.path()}, OutputFileName{QStringLiteral("photo.png")},
                         CollisionPolicy::Rename, {}, 3);

    REQUIRE_FALSE(result.isOk());
    REQUIRE(result.error() == IoError::WriteFailed);
}
