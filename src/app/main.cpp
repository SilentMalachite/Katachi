#include "app/MainWindow.hpp"
#include "core/CapabilityTable.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QString>

int main(int argc, char* argv[]) {
    const QApplication application(argc, argv);
    // QStringLiteral は使わない。マクロの定義ヘッダが Qt 6.8 と 6.11 で異なり、
    // clang-tidy の misc-include-cleaner が 6.8 でだけ「提供ヘッダが未 include」と判定する。
    // QString::fromUtf8 は <QString> が提供するため、両バージョンで安定する。
    QCoreApplication::setApplicationName(QString::fromUtf8("Katachi"));

    // 能力表は実行時に 1 度だけ作って注入する（docs/spec-core.md §3 / ADR-0007）。
    const katachi::core::CapabilityTable capabilities =
        katachi::core::CapabilityTable::buildFromQt();

    katachi::app::MainWindow window(capabilities);
    window.show();

    return QApplication::exec();
}
