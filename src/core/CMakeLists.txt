# docs/spec-core.md §1: core 層の依存は Qt6::Core / Qt6::Gui のみ。
# Qt6::Widgets は絶対にリンクしない。これにより
#   #include <QWidget> のようなフラットヘッダは include パスに無く、コンパイル時に落ちる。
# 不変条件スキャナ INV4 はテキスト上の検出を担い、リンク構成が実体を担保する。
#
# T1 時点の core はヘッダのみ（Concepts.hpp / Result.hpp）のため INTERFACE ライブラリ。
# FormatId.cpp が入る T2 で STATIC へ変更する。
add_library(katachi_core INTERFACE)

target_include_directories(katachi_core INTERFACE "${PROJECT_SOURCE_DIR}/src")

target_link_libraries(katachi_core INTERFACE Qt6::Core Qt6::Gui)
