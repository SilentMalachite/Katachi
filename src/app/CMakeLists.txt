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

# ── Phase 4 T2: 配布物の形とメタデータ ────────────────────────────────
#
# 実行ファイル名は Katachi（ターゲット名 katachi と区別する）。利用者から見える
# 名前であり、macOS では Katachi.app、Windows では Katachi.exe になる。
set(KATACHI_EXECUTABLE_NAME "${PROJECT_NAME}")

set_target_properties(katachi PROPERTIES OUTPUT_NAME "${KATACHI_EXECUTABLE_NAME}")

# **qt_add_executable は MACOSX_BUNDLE も WIN32_EXECUTABLE も既定では設定しない。**
# Qt の公式ドキュメントは「引数は add_executable へそのまま渡される」と述べており、
# T1 の実測でも macOS は素の実行ファイル、Windows は subsystem 3 (Windows CUI)
# だった。**既定に頼らず両方を明示する。**
set_target_properties(katachi PROPERTIES MACOSX_BUNDLE TRUE WIN32_EXECUTABLE TRUE)

if(APPLE)
    configure_file("${PROJECT_SOURCE_DIR}/packaging/macos/Info.plist.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/Info.plist" @ONLY)

    set_target_properties(katachi PROPERTIES MACOSX_BUNDLE_INFO_PLIST
                                             "${CMAKE_CURRENT_BINARY_DIR}/Info.plist")

    # アイコンは Contents/Resources へ入れる。Info.plist の CFBundleIconFile が指す。
    set(katachi_icns "${PROJECT_SOURCE_DIR}/packaging/icons/katachi.icns")
    target_sources(katachi PRIVATE "${katachi_icns}")
    set_source_files_properties("${katachi_icns}" PROPERTIES MACOSX_PACKAGE_LOCATION
                                                             Resources)
endif()

# ── 配布物の据え付け（ADR-0015 D5）────────────────────────────────────
#
# macOS は Katachi.app を、Windows は Katachi.exe を**接頭辞の直下**へ置く。
# Windows で bin/ を挟まないのは、windeployqt が DLL を実行ファイルの隣に
# 並べる前提だからである（T1 の実測。stage/Katachi.exe と stage/*.dll）。
#
# 署名・dmg 化（T5）と windeployqt・zip・インストーラ（T6）は、この据え付けた木を
# 入力にする。**プリセットに依らず常に定義する。** dev の木でも試せるようにするため。
install(
    TARGETS katachi
    BUNDLE DESTINATION .
    RUNTIME DESTINATION .)

if(WIN32)
    enable_language(RC)

    configure_file("${PROJECT_SOURCE_DIR}/packaging/windows/katachi.rc.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/katachi.rc" @ONLY)

    # .rc の ICON 文は自身と同じディレクトリを起点に解決される。複製して隣に置く。
    file(COPY "${PROJECT_SOURCE_DIR}/packaging/icons/katachi.ico"
         DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

    target_sources(katachi PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/katachi.rc")
endif()
