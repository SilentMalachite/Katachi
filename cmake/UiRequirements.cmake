# UI 非機能要件の検査（docs/spec-core.md §7 / ADR-0010）を ctest に登録する。
#
# **不変条件スキャナ（cmake/QualityGates.cmake）とは別のファイルに置く。**
# あちらは CLAUDE.md の「絶対禁止」を機械化したもので、由来が違う（ADR-0010）。
#
# 各検査につき 2 本を登録する。不変条件スキャナと同じ考え方で、
# 空振りしていないことを違反フィクスチャで確認する。

set(KATACHI_UI_SCRIPT "${PROJECT_SOURCE_DIR}/tests/ui/scan_ui_requirements.cmake")
set(KATACHI_UI_FIXTURES "${PROJECT_SOURCE_DIR}/tests/ui/fixtures/violations")

set(KATACHI_UI_CHECKS UI1 UI2 UI3)

foreach(check IN LISTS KATACHI_UI_CHECKS)
    string(TOLOWER "${check}" check_lower)

    add_test(NAME ui.${check_lower}
             COMMAND "${CMAKE_COMMAND}" -DKATACHI_UI_CHECK=${check}
                     -DKATACHI_UI_ROOT=${PROJECT_SOURCE_DIR}/src -P "${KATACHI_UI_SCRIPT}")

    add_test(NAME ui.${check_lower}.detects_violation
             COMMAND "${CMAKE_COMMAND}" -DKATACHI_UI_CHECK=${check}
                     -DKATACHI_UI_ROOT=${KATACHI_UI_FIXTURES}/${check_lower} -P
                     "${KATACHI_UI_SCRIPT}")

    set_tests_properties(ui.${check_lower}.detects_violation PROPERTIES WILL_FAIL TRUE)
endforeach()
