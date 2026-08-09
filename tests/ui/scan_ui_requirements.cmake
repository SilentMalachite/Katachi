# UI 非機能要件の機械検査（docs/spec-core.md §7 / ADR-0010）。
#
# **不変条件スキャナ（tests/invariants/scan_invariants.cmake）とは別に置く。**
# あちらは CLAUDE.md の「絶対禁止」を機械化したもので、由来が違う。
# 混ぜると、どちらの根拠で落ちているのか分からなくなる（ADR-0010）。
#
# 使い方:
#   cmake -DKATACHI_UI_CHECK=<CHECK> -DKATACHI_UI_ROOT=<src 相当のディレクトリ> \
#         -P tests/ui/scan_ui_requirements.cmake
#
# CHECK の一覧（§7 の 7 項目のうち、機械で確かめられる 3 つ）:
#   UI1  アニメーション・フェード・スライドを使っていない
#   UI2  独自のテーマを作っていない（ダークモードはシステム設定に追随する）
#   UI3  ウィンドウが単一（フローティングパネルを作らない）

cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED KATACHI_UI_CHECK)
    message(FATAL_ERROR "KATACHI_UI_CHECK is required")
endif()
if(NOT DEFINED KATACHI_UI_ROOT)
    message(FATAL_ERROR "KATACHI_UI_ROOT is required")
endif()

file(REAL_PATH "${KATACHI_UI_ROOT}" KATACHI_UI_ROOT BASE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

if(NOT IS_DIRECTORY "${KATACHI_UI_ROOT}")
    # ルートの指定ミスで「違反 0 件」と誤報告するのを防ぐ。
    message(FATAL_ERROR "KATACHI_UI_ROOT is not a directory: ${KATACHI_UI_ROOT}")
endif()

if(KATACHI_UI_CHECK STREQUAL "UI1")
    set(pattern "QPropertyAnimation|QVariantAnimation|QAbstractAnimation|QGraphicsEffect|QTimeLine")
    set(reason "アニメーション（docs/spec-core.md §7 で禁止。視覚・前庭系の負荷を避けるため）")
elseif(KATACHI_UI_CHECK STREQUAL "UI2")
    set(pattern "setStyleSheet|setPalette|QStyleFactory")
    set(reason "独自テーマ（§7: ダークモードはシステム設定に追随する）")
elseif(KATACHI_UI_CHECK STREQUAL "UI3")
    set(pattern "QDockWidget|QMdiArea|QMdiSubWindow")
    set(reason "フローティングパネル / 複数ウィンドウ（§7: ウィンドウは単一）")
else()
    message(FATAL_ERROR "unknown KATACHI_UI_CHECK: ${KATACHI_UI_CHECK}")
endif()

file(GLOB_RECURSE files "${KATACHI_UI_ROOT}/*.cpp" "${KATACHI_UI_ROOT}/*.hpp")

set(violations "")

foreach(file IN LISTS files)
    file(RELATIVE_PATH rel "${KATACHI_UI_ROOT}" "${file}")
    file(READ "${file}" content)

    # コメントは落とす。禁止対象を説明する文章まで違反にしないため
    # （INV3A / INV4 と同じ扱い。INV6 だけは抑制指示がコメントで書かれるため落とさない）。
    string(REGEX REPLACE "/\\*[^*]*\\*+([^/*][^*]*\\*+)*/" "" content "${content}")
    string(REGEX REPLACE "//[^\n]*" "" content "${content}")

    if(content MATCHES "${pattern}")
        list(APPEND violations "  ${rel}: ${reason}")
    endif()
endforeach()

list(LENGTH violations violation_count)
if(NOT violation_count EQUAL 0)
    list(JOIN violations "\n" report)
    message(FATAL_ERROR "[${KATACHI_UI_CHECK}] UI 非機能要件の違反 ${violation_count} 件\n${report}")
endif()

message(STATUS "[${KATACHI_UI_CHECK}] ok (${KATACHI_UI_ROOT})")
