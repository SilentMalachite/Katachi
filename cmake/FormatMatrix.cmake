# docs/format-matrix.md をビルド時に自動生成する（docs/phases.md §4 Phase 1）。
#
# 能力表は実行環境の Qt プラグイン構成で決まるため、生成物はリポジトリに
# コミットしない（.gitignore 済み）。生成器が自分で中身を検証し、
# 表が空だったり PNG が無かったりすれば非ゼロ終了してビルドを止める。

set(KATACHI_FORMAT_MATRIX "${PROJECT_SOURCE_DIR}/docs/format-matrix.md")

add_executable(katachi_format_matrix "${PROJECT_SOURCE_DIR}/tools/format_matrix.cpp")

target_link_libraries(katachi_format_matrix PRIVATE katachi_core katachi_warnings)

add_custom_command(
    OUTPUT "${KATACHI_FORMAT_MATRIX}"
    COMMAND katachi_format_matrix "${KATACHI_FORMAT_MATRIX}"
    DEPENDS katachi_format_matrix
    COMMENT "docs/format-matrix.md を生成"
    VERBATIM)

add_custom_target(katachi_format_matrix_doc ALL DEPENDS "${KATACHI_FORMAT_MATRIX}")

# テストからも生成物を検証する。tests は先に add_subdirectory されているため、
# 依存とマクロ定義はここ（生成ターゲットの定義後）で足す。
add_dependencies(katachi_tests katachi_format_matrix_doc)

target_compile_definitions(katachi_tests PRIVATE KATACHI_FORMAT_MATRIX="${KATACHI_FORMAT_MATRIX}")
