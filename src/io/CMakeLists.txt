# docs/spec-core.md §1: io は副作用の境界。core に依存してよいが逆は不可。
#
# **Qt6::Widgets はリンクしない。** io はワーカースレッドで動く層であり（ADR-0010）、
# リンクしなければ #include <QWidget> のようなフラットヘッダは include パスに無く、
# コンパイル時に落ちる。不変条件スキャナ INV7 はテキスト上の検出を担う。
# 両方が揃って初めて「ワーカーから QWidget に触れていない」が塞がる
# （docs/phases.md §4 Phase 2 の受け入れ基準）。Phase 1 の katachi_core と同じ手口。
#
# T2 で FileSource.cpp / FileSink.cpp が入ったため INTERFACE から STATIC へ変更した。
add_library(
    katachi_io STATIC
    CollisionPolicy.cpp
    CollisionPolicy.hpp
    FileSink.cpp
    FileSink.hpp
    FileSource.cpp
    FileSource.hpp
    IoConcepts.hpp
    IoError.hpp
    JobRunner.hpp)

target_include_directories(katachi_io PUBLIC "${PROJECT_SOURCE_DIR}/src")

target_link_libraries(
    katachi_io
    PUBLIC Qt6::Core katachi_core
    PRIVATE katachi_warnings katachi_sanitizers)
