// 自動生成された docs/format-matrix.md の検証。
//
// docs/phases.md §4 Phase 1 の「docs/format-matrix.md がビルド時に自動生成される」を
// テストからも確認する。生成器自身も中身を検証するが、それはビルド時の話であり、
// ここでは「生成物が実際に置かれ、能力表の内容を反映しているか」を見る。
//
// テスト名は ASCII に限る。
#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QString>

#include <catch2/catch_test_macros.hpp>

namespace {

QString readMatrix() {
    QFile file(QString::fromUtf8(KATACHI_FORMAT_MATRIX));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

} // namespace

TEST_CASE("the format matrix is generated and not empty", "[docs][matrix]") {
    const QString matrix = readMatrix();

    REQUIRE_FALSE(matrix.isEmpty());
    REQUIRE(matrix.contains(QStringLiteral("| フォーマット | 拡張子 |")));
}

TEST_CASE("the format matrix says it is generated", "[docs][matrix]") {
    // 手で編集されないよう、生成物である旨が本文に必ず入っていること。
    const QString matrix = readMatrix();

    REQUIRE(matrix.contains(QStringLiteral("ビルド時に自動生成")));
    REQUIRE(matrix.contains(QStringLiteral("tools/format_matrix.cpp")));
}

TEST_CASE("the format matrix lists png as readable and writable", "[docs][matrix]") {
    const QString matrix = readMatrix();

    REQUIRE(matrix.contains(QStringLiteral("| `png` | png | o | o |")));
}

TEST_CASE("the format matrix shows merged alias extensions", "[docs][matrix]") {
    // ADR-0006 の別名統合が一覧に反映されていること。
    // jpeg の行に jpg と jfif が拡張子として並ぶ。
    const QString matrix = readMatrix();

    REQUIRE(matrix.contains(QStringLiteral("| `jpeg` | jfif, jpeg, jpg |")));
    REQUIRE_FALSE(matrix.contains(QStringLiteral("| `jpg` |")));
    REQUIRE_FALSE(matrix.contains(QStringLiteral("| `jfif` |")));
}

TEST_CASE("the format matrix explains what a dash means", "[docs][matrix]") {
    // ADR-0007: 書き出せない形式の「-」は「判定していない」であって
    // 「その性質が無い」ではない。この区別が読者に伝わること。
    const QString matrix = readMatrix();

    REQUIRE(matrix.contains(QStringLiteral("判定していない")));
}
