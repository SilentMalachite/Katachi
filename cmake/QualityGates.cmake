# 不変条件スキャナ（docs/phases.md §3）を ctest に登録する。
#
# 各検査につき 2 本を登録する。
#   invariant.<check>                    src/ を走査し、違反が無いことを確認する
#   invariant.<check>.detects_violation  故意に違反したフィクスチャを走査し、
#                                        スキャナが確実に落ちることを確認する
#
# 後者が無いと、スキャナが空振りしていても「全部 green」に見えてしまう。
# docs/phases.md §2.2 の concept 適合テストが否定側の static_assert を要求するのと同じ理由。

set(KATACHI_SCAN_SCRIPT "${PROJECT_SOURCE_DIR}/tests/invariants/scan_invariants.cmake")
set(KATACHI_SCAN_FIXTURES "${PROJECT_SOURCE_DIR}/tests/invariants/fixtures/violations")

set(KATACHI_INVARIANT_CHECKS INV1 INV2 INV3A INV3B INV4 INV5 INV6)

foreach(check IN LISTS KATACHI_INVARIANT_CHECKS)
    string(TOLOWER "${check}" check_lower)

    add_test(NAME invariant.${check_lower}
             COMMAND "${CMAKE_COMMAND}" -DKATACHI_SCAN_CHECK=${check}
                     -DKATACHI_SCAN_ROOT=${PROJECT_SOURCE_DIR}/src -P "${KATACHI_SCAN_SCRIPT}")

    add_test(NAME invariant.${check_lower}.detects_violation
             COMMAND "${CMAKE_COMMAND}" -DKATACHI_SCAN_CHECK=${check}
                     -DKATACHI_SCAN_ROOT=${KATACHI_SCAN_FIXTURES}/${check_lower} -P
                     "${KATACHI_SCAN_SCRIPT}")

    set_tests_properties(invariant.${check_lower}.detects_violation PROPERTIES WILL_FAIL TRUE)
endforeach()
