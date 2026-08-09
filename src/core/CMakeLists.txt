# docs/spec-core.md §1: core 層の依存は Qt6::Core / Qt6::Gui のみ。
# Qt6::Widgets は絶対にリンクしない。これにより
#   #include <QWidget> のようなフラットヘッダは include パスに無く、コンパイル時に落ちる。
# 不変条件スキャナ INV4 はテキスト上の検出を担い、リンク構成が実体を担保する。
#
# T3 で CapabilityTable.cpp が入ったため INTERFACE から STATIC へ変更した。
add_library(
    katachi_core STATIC
    CapabilityTable.cpp
    CapabilityTable.hpp
    Concepts.hpp
    ConvertError.hpp
    FormatId.hpp
    Result.hpp)

target_include_directories(katachi_core PUBLIC "${PROJECT_SOURCE_DIR}/src")

target_link_libraries(
    katachi_core
    PUBLIC Qt6::Core Qt6::Gui
    PRIVATE katachi_warnings katachi_sanitizers)
