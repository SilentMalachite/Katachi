# 配布物の機械検査 P1〜P8 を ctest に登録する（Phase 4 T7）。
#
# **`KATACHI_PACKAGE=ON`（release プリセット）のときだけ有効。**
# 検査対象は「実際に組み立てた配布物」であり、dev の木には存在しない。
#
# cmake/QualityGates.cmake と同じ形で、各検査につき 2 本を登録する。
#   package.<check>                    組み立てた配布物を検査し、通ることを確認する
#   package.<check>.detects_violation  故意に壊した木を検査し、確実に落ちることを確認する
#
# 後者が無いと、スキャナが空振りしていても「全部 green」に見えてしまう。

if(NOT KATACHI_PACKAGE)
    return()
endif()

set(KATACHI_PACKAGE_STAGE "${CMAKE_BINARY_DIR}/package")
set(KATACHI_PACKAGE_SCAN "${PROJECT_SOURCE_DIR}/tests/packaging/scan_package.cmake")

# Qt の接頭辞。P4 が「Qt に入っている画像プラグイン」と突き合わせるために要る。
get_filename_component(KATACHI_QT_PREFIX "${Qt6_DIR}/../../.." ABSOLUTE)

if(APPLE)
    set(katachi_package_script "${PROJECT_SOURCE_DIR}/cmake/PackageMacOS.cmake")
elseif(WIN32)
    set(katachi_package_script "${PROJECT_SOURCE_DIR}/cmake/PackageWindows.cmake")
else()
    message(STATUS "配布物の検査は macOS / Windows のみ")
    return()
endif()

# ── 組み立てを 1 度だけ行い、検査はそれを見る ───────────────────────
#
# ctest のフィクスチャで順序を作る。**組み立てが落ちたら検査は走らず、
# 「検査が無かったので green」にはならない**（ctest は依存が失敗すると
# 後続を not run として扱う）。
add_test(NAME package.build
         COMMAND "${CMAKE_COMMAND}"
                 -DKATACHI_STAGE_DIR=${KATACHI_PACKAGE_STAGE}
                 -DKATACHI_SOURCE_DIR=${PROJECT_SOURCE_DIR}
                 -DKATACHI_QT_PREFIX=${KATACHI_QT_PREFIX}
                 -DKATACHI_APP_VERSION=${PROJECT_VERSION}
                 -DKATACHI_QT_VERSION=${Qt6_VERSION}
                 -P "${katachi_package_script}")

# install したものを組み立ての入力にする。ctest から cmake --install を呼ぶ。
add_test(NAME package.install
         COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}"
                 --prefix "${KATACHI_PACKAGE_STAGE}")

set_tests_properties(package.install PROPERTIES FIXTURES_SETUP katachi_installed)
set_tests_properties(package.build PROPERTIES FIXTURES_REQUIRED katachi_installed
                                              FIXTURES_SETUP katachi_packaged)

set(KATACHI_PACKAGE_CHECKS P1 P2 P3 P4 P5 P6 P7 P8 P9)

# **P8（実際に起動する）は macOS でのみ意味がある。**
# Windows のローダーは DLL が欠けたときエラーダイアログを出してプロセスを
# 生かしたまま待つため、「5 秒生存した」が「起動した」の根拠にならない
# （CI run 31446352005 の違反フィクスチャが暴いた）。
# Windows の起動可能性は P9（依存がすべて配布物の中で解決する）が担う。
if(NOT APPLE)
    list(REMOVE_ITEM KATACHI_PACKAGE_CHECKS P8)
endif()

# P2 / P3 は macOS 固有（universal / minos）。Windows では検査自体が
# 対象外として素通りするため、違反フィクスチャも作らない。
set(KATACHI_PACKAGE_VIOLATIONS ${KATACHI_PACKAGE_CHECKS})
if(NOT APPLE)
    list(REMOVE_ITEM KATACHI_PACKAGE_VIOLATIONS P2 P3)
endif()

foreach(check IN LISTS KATACHI_PACKAGE_CHECKS)
    string(TOLOWER "${check}" lower)
    add_test(NAME package.${lower}
             COMMAND "${CMAKE_COMMAND}"
                     -DKATACHI_CHECK=${check}
                     -DKATACHI_PACKAGE_ROOT=${KATACHI_PACKAGE_STAGE}
                     -DKATACHI_QT_PREFIX=${KATACHI_QT_PREFIX}
                     -DKATACHI_EXPECTED_VERSION=${PROJECT_VERSION}
                     -P "${KATACHI_PACKAGE_SCAN}")
    set_tests_properties(package.${lower} PROPERTIES FIXTURES_REQUIRED katachi_packaged)
endforeach()

# 故意に壊した配布物を検査させ、確実に落ちることを確かめる。
# **これが無いと、検査が空振りしていても「全部 green」に見える。**
foreach(check IN LISTS KATACHI_PACKAGE_VIOLATIONS)
    string(TOLOWER "${check}" lower)
    add_test(NAME package.${lower}.detects_violation
             COMMAND "${CMAKE_COMMAND}"
                     -DKATACHI_CHECK=${check}
                     -DKATACHI_PACKAGE_ROOT=${KATACHI_PACKAGE_STAGE}
                     -DKATACHI_WORK_DIR=${CMAKE_BINARY_DIR}/package-violations
                     -DKATACHI_SCAN=${KATACHI_PACKAGE_SCAN}
                     -DKATACHI_QT_PREFIX=${KATACHI_QT_PREFIX}
                     -DKATACHI_EXPECTED_VERSION=${PROJECT_VERSION}
                     -P "${PROJECT_SOURCE_DIR}/tests/packaging/violate_package.cmake")
    set_tests_properties(package.${lower}.detects_violation
                         PROPERTIES FIXTURES_REQUIRED katachi_packaged)
endforeach()
