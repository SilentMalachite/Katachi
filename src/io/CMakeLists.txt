# docs/spec-core.md §1: io は副作用の境界。core に依存してよいが逆は不可。
#
# **Qt6::Widgets はリンクしない。** io はワーカースレッドで動く層であり（ADR-0010）、
# リンクしなければ #include <QWidget> のようなフラットヘッダは include パスに無く、
# コンパイル時に落ちる。不変条件スキャナ INV7 はテキスト上の検出を担う。
# 両方が揃って初めて「ワーカーから QWidget に触れていない」が塞がる
# （docs/phases.md §4 Phase 2 の受け入れ基準）。Phase 1 の katachi_core と同じ手口。
#
# T1 時点では IoError.hpp / IoConcepts.hpp のヘッダのみのため INTERFACE。
# STATIC にするとソースが無くて CMake が失敗する。
# **FileSource.cpp / FileSink.cpp が入る T2 で STATIC へ変更する。**
add_library(katachi_io INTERFACE)

target_include_directories(katachi_io INTERFACE "${PROJECT_SOURCE_DIR}/src")

target_link_libraries(katachi_io INTERFACE katachi_core)
