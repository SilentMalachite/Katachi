#include "app/MainWindow.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QString>

int main(int argc, char* argv[]) {
    const QApplication application(argc, argv);
    // QStringLiteral は使わない。マクロの定義ヘッダが Qt 6.8 と 6.11 で異なり、
    // clang-tidy の misc-include-cleaner が 6.8 でだけ「提供ヘッダが未 include」と判定する。
    // QString::fromUtf8 は <QString> が提供するため、両バージョンで安定する。
    QCoreApplication::setApplicationName(QString::fromUtf8("Katachi"));

    katachi::app::MainWindow window;
    window.show();

    return QApplication::exec();
}
