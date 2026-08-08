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

// テスト名は ASCII に限る。catch_discover_tests はテスト名を
// そのままフィルタ引数として実行ファイルへ渡すため、非 ASCII 名は
// Windows のコンソール encoding で化けて "No test cases matched" になる。
TEST_CASE("QImageReader supports at least PNG", "[smoke]") {
    const QList<QByteArray> readable = QImageReader::supportedImageFormats();

    REQUIRE_FALSE(readable.isEmpty());
    REQUIRE(readable.contains(QByteArrayLiteral("png")));
}

TEST_CASE("QImageWriter supports at least PNG", "[smoke]") {
    const QList<QByteArray> writable = QImageWriter::supportedImageFormats();

    REQUIRE_FALSE(writable.isEmpty());
    REQUIRE(writable.contains(QByteArrayLiteral("png")));
}
