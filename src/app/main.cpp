#include "app/MainWindow.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QString>

int main(int argc, char* argv[]) {
    const QApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Katachi"));

    katachi::app::MainWindow window;
    window.show();

    return QApplication::exec();
}
