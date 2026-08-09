# Q_OBJECT を含むため moc が必要。AUTOMOC はこのディレクトリにのみ効かせる
# （FetchContent で取得する Catch2 に波及させないため、グローバルには設定しない）。
set(CMAKE_AUTOMOC ON)

# app 層をライブラリに分け、実行ファイルは薄い main.cpp のみとする。
# Phase 2 で app 層をテストから駆動できるようにするための分割。
add_library(
    katachi_app STATIC
    JobTableModel.cpp
    JobTableModel.hpp
    SettingsPanel.cpp
    SettingsPanel.hpp
    MainWindow.hpp
    MainWindow.cpp)

target_include_directories(katachi_app PUBLIC "${PROJECT_SOURCE_DIR}/src")

# katachi_io は katachi_core を伝播する。両方書くと同じアーカイブが
# リンク行に 2 回並び、Apple ld が duplicate libraries の警告を出す（T1 の知見）。
target_link_libraries(
    katachi_app
    PUBLIC Qt6::Widgets katachi_io
    PRIVATE katachi_warnings katachi_sanitizers)

qt_add_executable(katachi main.cpp)

target_link_libraries(katachi PRIVATE katachi_app katachi_warnings katachi_sanitizers)
