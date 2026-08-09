// FileSource / FileSink のテスト（Phase 2 T2）。
//
// io 層はファイルシステムに触れてよい層なので、実ファイルで検証する。
// 一時ディレクトリは QTemporaryDir が破棄時に片付ける。
//
// テスト名は ASCII に限る（Phase 0 の知見）。
#include "core/Result.hpp"
#include "io/FileSink.hpp"
#include "io/FileSource.hpp"
#include "io/IoError.hpp"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

namespace {

using katachi::io::FileSink;
using katachi::io::FileSource;
using katachi::io::IoError;

constexpr auto allEntries = QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden;

QString writeRawFile(const QDir& dir, const QString& name, const QByteArray& bytes) {
    const QString path = dir.filePath(name);
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    REQUIRE(file.write(bytes) == bytes.size());
    file.close();
    return path;
}

QByteArray readRawFile(const QString& path) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    return file.readAll();
}

} // namespace

TEST_CASE("FileSource reads back the bytes that were written", "[io][file]") {
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QByteArray payload("katachi io\x00 bytes", 17);
    const QString path = writeRawFile(QDir(temp.path()), QStringLiteral("input.bin"), payload);

    FileSource source(path);
    const auto result = source.read();

    REQUIRE(result.isOk());
    REQUIRE(result.value() == payload);
}

TEST_CASE("FileSource reports NotFound for a missing path", "[io][file]") {
    QTemporaryDir temp;
    REQUIRE(temp.isValid());

    FileSource source(QDir(temp.path()).filePath(QStringLiteral("nothing.bin")));
    const auto result = source.read();

    REQUIRE_FALSE(result.isOk());
    REQUIRE(result.error() == IoError::NotFound);
}

TEST_CASE("FileSource reports OpenFailed for a directory", "[io][file]") {
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    REQUIRE(QDir(temp.path()).mkdir(QStringLiteral("sub")));

    FileSource source(QDir(temp.path()).filePath(QStringLiteral("sub")));
    const auto result = source.read();

    REQUIRE_FALSE(result.isOk());
    REQUIRE(result.error() == IoError::OpenFailed);
}

TEST_CASE("FileSource reports TooLarge above the limit", "[io][file]") {
    // ADR-0008: 上限の判定は読み込みより前に行う。読んでから測ると意味が無い。
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QDir dir(temp.path());
    const QString atLimit = writeRawFile(dir, QStringLiteral("eight.bin"), QByteArray(8, 'a'));
    const QString overLimit = writeRawFile(dir, QStringLiteral("nine.bin"), QByteArray(9, 'a'));

    FileSource fits(atLimit, 8);
    FileSource tooBig(overLimit, 8);

    // ちょうど上限は通る。境界の向きを固定する。
    REQUIRE(fits.read().isOk());

    const auto result = tooBig.read();
    REQUIRE_FALSE(result.isOk());
    REQUIRE(result.error() == IoError::TooLarge);
}

TEST_CASE("FileSink writes the exact bytes", "[io][file]") {
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString path = QDir(temp.path()).filePath(QStringLiteral("out.bin"));
    const QByteArray payload("katachi\x00 sink", 12);

    FileSink sink(path);
    const auto result = sink.write(payload);

    REQUIRE(result.isOk());
    REQUIRE(readRawFile(path) == payload);
}

TEST_CASE("FileSink reports WriteFailed for a missing directory", "[io][file]") {
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString path =
        QDir(temp.path()).filePath(QStringLiteral("missing")) + QStringLiteral("/out.bin");

    FileSink sink(path);
    const auto result = sink.write(QByteArray("x"));

    REQUIRE_FALSE(result.isOk());
    REQUIRE(result.error() == IoError::WriteFailed);
}

TEST_CASE("FileSink leaves no partial file when it fails", "[io][file]") {
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QDir dir(temp.path());
    const QString path = dir.filePath(QStringLiteral("missing")) + QStringLiteral("/out.bin");
    const QStringList before = dir.entryList(allEntries);

    FileSink sink(path);
    const auto result = sink.write(QByteArray(1024, 'x'));

    REQUIRE_FALSE(result.isOk());
    REQUIRE_FALSE(QFileInfo::exists(path));
    // QSaveFile は出力先に一時ファイルを作ってから改名する。
    // 失敗したときに一時ファイルもディレクトリも残さないことを確認する。
    REQUIRE(dir.entryList(allEntries) == before);
}
