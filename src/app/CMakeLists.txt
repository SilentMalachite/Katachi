# Q_OBJECT を含むため moc が必要。AUTOMOC はこのディレクトリにのみ効かせる
# （FetchContent で取得する Catch2 に波及させないため、グローバルには設定しない）。
set(CMAKE_AUTOMOC ON)

# app 層をライブラリに分け、実行ファイルは薄い main.cpp のみとする。
# Phase 2 で app 層をテストから駆動できるようにするための分割。
add_library(katachi_app STATIC MainWindow.hpp MainWindow.cpp)

target_include_directories(katachi_app PUBLIC "${PROJECT_SOURCE_DIR}/src")

target_link_libraries(
    katachi_app
    PUBLIC Qt6::Widgets
    PRIVATE katachi_warnings katachi_sanitizers)

qt_add_executable(katachi main.cpp)

target_link_libraries(katachi PRIVATE katachi_app katachi_warnings katachi_sanitizers)
