// Phase 0 の smoke テスト。
// 能力表（CapabilityTable）は Phase 1 で作る。ここで確かめるのは
// 「両 OS の CI で Qt の画像フォーマットプラグインが実際に読み込まれること」だけ。
// これが落ちる環境では Phase 1 の能力表テストが全て無意味になる。
//
// テストコードはフォーマット名の文字列リテラル禁止の対象外（docs/spec-core.md §3）。
#include <QByteArray>
#include <QImageReader>
#include <QImageWriter>
#include <QList>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Qt が読み込める画像フォーマットが存在し、PNG を含む", "[smoke]") {
    const QList<QByteArray> readable = QImageReader::supportedImageFormats();

    REQUIRE_FALSE(readable.isEmpty());
    REQUIRE(readable.contains(QByteArrayLiteral("png")));
}

TEST_CASE("Qt が書き出せる画像フォーマットが存在し、PNG を含む", "[smoke]") {
    const QList<QByteArray> writable = QImageWriter::supportedImageFormats();

    REQUIRE_FALSE(writable.isEmpty());
    REQUIRE(writable.contains(QByteArrayLiteral("png")));
}
