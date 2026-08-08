// Catch2 の既定 main は使わず、QCoreApplication を先に構築する。
// Qt の画像フォーマットプラグインの探索がライブラリパスに依存するため。
#include <QCoreApplication>

#include <catch2/catch_session.hpp>

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    return Catch::Session().run(argc, argv);
}
