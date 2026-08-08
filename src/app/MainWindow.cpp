#include "app/MainWindow.hpp"

#include <QCoreApplication>
#include <QMainWindow>
#include <QWidget>

namespace katachi::app {
namespace {

// docs/spec-core.md §7: ウィンドウは単一。起動時の大きさのみ決めておく。
constexpr int initialWindowWidth = 960;
constexpr int initialWindowHeight = 600;

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QCoreApplication::applicationName());
    resize(initialWindowWidth, initialWindowHeight);
}

} // namespace katachi::app
