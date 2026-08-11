# 成果物へ同梱する third_party_licenses.txt を組み立てる（ADR-0016 / 受け入れ基準 3）。
#
# `cmake -P` のスクリプトモードで走る。要る変数:
#   KATACHI_LICENSE_DIR   packaging/licenses
#   KATACHI_OUTPUT        書き出す third_party_licenses.txt のパス
#   KATACHI_APP_VERSION   成果物の版（project(VERSION) 由来）
#   KATACHI_QT_VERSION    リンクした Qt の版
#
# **法文が 1 つでも欠けたら FATAL_ERROR で止まる。** 黙って不完全な
# 権利表示を配らないため（cmake/CollectExtraCodecs.cmake と同じ考え方）。

cmake_minimum_required(VERSION 3.24)

foreach(required IN ITEMS KATACHI_LICENSE_DIR KATACHI_OUTPUT KATACHI_APP_VERSION
                          KATACHI_QT_VERSION)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "ThirdPartyLicenses.cmake: ${required} が渡されていない")
    endif()
endforeach()

set(manifest "${KATACHI_LICENSE_DIR}/qt-third-party.json")
set(spdx_dir "${KATACHI_LICENSE_DIR}/spdx")

if(NOT EXISTS "${manifest}")
    message(FATAL_ERROR
            "同梱物の一覧が無い: ${manifest}\n"
            "packaging/licenses/from-qt-sbom.py を走らせて作ること。")
endif()

file(READ "${manifest}" manifest_json)
string(JSON qt_sbom_version GET "${manifest_json}" "qtVersion")
string(JSON limitation GET "${manifest_json}" "limitation")
string(JSON shipped_count LENGTH "${manifest_json}" "shipped")

# SBOM に現れないが同梱されるもの（Qt がビルド済みバイナリとして持つ Mesa など）。
# **手で管理する受け皿であり、1 件ごとに「なぜ SBOM に無いか」が書いてある。**
set(extra "${KATACHI_LICENSE_DIR}/extra-components.json")
set(extra_count 0)
if(EXISTS "${extra}")
    file(READ "${extra}" extra_json)
    string(JSON extra_count LENGTH "${extra_json}" "shipped")
endif()

# ライセンス式のうち OR を持つものは、**本体の GPLv3-or-later と両立する側を選ぶ**。
# 選ばなかった側の法文は同梱しない。理由は packaging/licenses/SOURCES.md にもある。
set(katachi_or_choice
    "FTL OR GPL-2.0-only|FTL"                    # GPL-2.0-only は GPLv3 と両立しない
    "CC0-1.0 OR Apache-2.0|CC0-1.0"              # より制約の少ない側
    "AFL-2.1 OR GPL-2.0-or-later|GPL-2.0-or-later") # AFL-2.1 は GPL と両立しない

# ライセンス式 -> 必要な SPDX 識別子の一覧
function(katachi_resolve_license expression out_ids out_note)
    set(note "")
    foreach(pair IN LISTS katachi_or_choice)
        string(REPLACE "|" ";" parts "${pair}")
        list(GET parts 0 from)
        list(GET parts 1 to)
        if(expression STREQUAL from)
            set(expression "${to}")
            set(note "「${from}」のうち ${to} を選択")
            break()
        endif()
    endforeach()
    string(REPLACE " AND " ";" ids "${expression}")
    string(REPLACE " OR " ";" ids "${ids}")
    set(${out_ids} "${ids}" PARENT_SCOPE)
    set(${out_note} "${note}" PARENT_SCOPE)
endfunction()

# ── 本文を組み立てる ────────────────────────────────────────────────
set(body "")
string(APPEND body
"Katachi ${KATACHI_APP_VERSION} — 第三者コードの権利表示
================================================================

このファイルは Katachi の配布物に同梱される第三者コードの一覧と、
それぞれのライセンス条文です。

Katachi 本体は GNU General Public License v3.0 or later で配布されます。
本体の条文は同梱の LICENSE を参照してください。
対応するソースコードの入手先:
    https://github.com/SilentMalachite/Katachi

このファイルは packaging/licenses/ の内容から機械的に生成されます。
手で編集しないでください。


1. Qt ${KATACHI_QT_VERSION}
----------------------------------------------------------------

Katachi は Qt 6 を GNU Lesser General Public License v3 の条件で利用し、
**動的リンクのみ**を行います。静的リンクは行いません。

LGPLv3 は、利用者が Qt を差し替えて再リンクできることを求めます。
本配布物は Qt を独立した共有ライブラリ（macOS はフレームワーク、
Windows は DLL）として同梱しているため、それらを利用者自身がビルドした
ものへ置き換えられます。

  Qt の版:     ${KATACHI_QT_VERSION}
  入手先:      https://download.qt.io/
  改変の有無:  Katachi は Qt に改変を加えていません。

  macOS での注意: 配布する .app は Developer ID で署名されています。
  同梱の Qt フレームワークを差し替えると署名が壊れ、Gatekeeper が
  起動を拒みます。差し替えたあとは次のように再署名してください。

      codesign --force --deep --sign - Katachi.app

LGPLv3 の条文はこのファイルの末尾（付録 A）にあります。
LGPLv3 は GPLv3 を参照します。GPLv3 の条文は同梱の LICENSE にあります。


2. Qt が内部に含む第三者コード
----------------------------------------------------------------

以下は Qt ${qt_sbom_version} の SBOM（SPDX 2.3）から機械的に列挙した、
**本配布物に実際に同梱されるもの**です。

")

# ── 一覧と、必要な法文の収集 ────────────────────────────────────────
set(needed_ids "")
set(missing "")
set(emitted 0)

# 1 件を本文へ書き出す。**JSON をリストに入れない。** 著作権表示に `;` が
# 含まれると CMake のリストが壊れるため、添字で回して都度取り出す。
macro(katachi_emit_entry json index)
    string(JSON _item GET "${${json}}" "shipped" ${index})
    string(JSON _name GET "${_item}" "name")
    string(JSON _version GET "${_item}" "version")
    string(JSON _expression GET "${_item}" "license")
    string(JSON _copyright GET "${_item}" "copyright")
    string(JSON _download GET "${_item}" "download")
    string(JSON _why ERROR_VARIABLE _why_error GET "${_item}" "why_not_in_sbom")

    # platforms がある項目は、その配布物にだけ載せる。
    # 入っていないものを権利表示に並べない（法的な文書なので正確に保つ）。
    string(JSON _plat_count ERROR_VARIABLE _plat_error LENGTH "${_item}" "platforms")
    if(NOT _plat_error AND DEFINED KATACHI_PLATFORM)
        set(_wanted FALSE)
        math(EXPR _plat_last "${_plat_count} - 1")
        foreach(_p RANGE ${_plat_last})
            string(JSON _plat GET "${_item}" "platforms" ${_p})
            if(_plat STREQUAL "${KATACHI_PLATFORM}")
                set(_wanted TRUE)
            endif()
        endforeach()
        if(NOT _wanted)
            set(_skip TRUE)
        else()
            set(_skip FALSE)
        endif()
    else()
        set(_skip FALSE)
    endif()

    if(NOT _skip)
    math(EXPR emitted "${emitted} + 1")
    katachi_resolve_license("${_expression}" _ids _choice_note)

    string(APPEND body "  ${_name} (${_version})\n")
    string(APPEND body "    ライセンス: ${_expression}\n")
    if(_choice_note)
        string(APPEND body "                ${_choice_note}\n")
    endif()
    if(_download)
        string(APPEND body "    入手先:     ${_download}\n")
    endif()
    string(REPLACE "\n" "\n                " _indented "${_copyright}")
    string(APPEND body "    著作権:     ${_indented}\n")
    if(NOT _why_error)
        string(REPLACE "\n" "\n                " _why_indented "${_why}")
        string(APPEND body "    SBOM 外:    ${_why_indented}\n")
    endif()
    string(APPEND body "\n")

    foreach(_id IN LISTS _ids)
        if(NOT EXISTS "${spdx_dir}/${_id}.txt")
            list(APPEND missing "${_id} (${_name} が必要とする)")
        else()
            list(APPEND needed_ids "${_id}")
        endif()
    endforeach()
    endif()
endmacro()

math(EXPR last "${shipped_count} - 1")
foreach(index RANGE ${last})
    katachi_emit_entry(manifest_json ${index})
endforeach()

if(extra_count GREATER 0)
    string(APPEND body
"
  --- ここから下は Qt の SBOM に現れないもの（手で管理している）---

")
    math(EXPR extra_last "${extra_count} - 1")
    foreach(index RANGE ${extra_last})
        katachi_emit_entry(extra_json ${index})
    endforeach()
endif()

# Qt 本体の LGPLv3 も必要。
if(NOT EXISTS "${spdx_dir}/LGPL-3.0-only.txt")
    list(APPEND missing "LGPL-3.0-only (Qt 本体が必要とする)")
else()
    list(APPEND needed_ids "LGPL-3.0-only")
endif()

if(missing)
    list(REMOVE_DUPLICATES missing)
    string(REPLACE ";" "\n  - " pretty "${missing}")
    message(FATAL_ERROR
            "同梱物が必要とする法文が ${spdx_dir} に無い:\n  - ${pretty}\n\n"
            "packaging/licenses/fetch-spdx-texts.py を走らせて取得すること。\n"
            "**法文が欠けたまま配布物を作らない。**")
endif()

list(REMOVE_DUPLICATES needed_ids)
list(SORT needed_ids CASE INSENSITIVE)

string(APPEND body
"
3. この一覧の限界（偽らないために記す）
----------------------------------------------------------------

${limitation}


4. ライセンス条文
----------------------------------------------------------------

")

set(appendix_index 0)
foreach(id IN LISTS needed_ids)
    file(READ "${spdx_dir}/${id}.txt" text)
    if(id STREQUAL "LGPL-3.0-only")
        string(APPEND body "\n付録 A: ${id}\n")
    else()
        string(APPEND body "\n---- ${id} ----------------------------------------------\n\n")
    endif()
    string(APPEND body "${text}\n")
endforeach()

list(LENGTH needed_ids id_count)
file(WRITE "${KATACHI_OUTPUT}" "${body}")
message(STATUS "third_party_licenses.txt を書き出した: 第三者 ${emitted} 件 / 法文 ${id_count} 種")
