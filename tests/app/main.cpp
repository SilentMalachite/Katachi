// app 層のテストは QApplication を必要とするため、実行ファイルを core / io と分ける。
//
// **分けることに意味がある。** katachi_tests（core と io）は Qt6::Widgets を
// リンクしない。1 つにまとめると、io が Widgets を引いていなくてもテスト
// バイナリ経由で見えてしまい、「ワーカー側の層は Widgets に触れない」という
// 担保が弱くなる（docs/phases.md §4 Phase 2 の受け入れ基準）。
#include <QApplication>

#include <catch2/catch_session.hpp>

int main(int argc, char* argv[]) {
    const QApplication application(argc, argv);
    return Catch::Session().run(argc, argv);
}
