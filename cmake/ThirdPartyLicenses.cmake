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
math(EXPR last "${shipped_count} - 1")

foreach(index RANGE ${last})
    string(JSON item GET "${manifest_json}" "shipped" ${index})
    string(JSON name GET "${item}" "name")
    string(JSON version GET "${item}" "version")
    string(JSON expression GET "${item}" "license")
    string(JSON copyright GET "${item}" "copyright")
    string(JSON download GET "${item}" "download")

    katachi_resolve_license("${expression}" ids choice_note)

    string(APPEND body "  ${name} (${version})\n")
    string(APPEND body "    ライセンス: ${expression}\n")
    if(choice_note)
        string(APPEND body "                ${choice_note}\n")
    endif()
    if(download)
        string(APPEND body "    入手先:     ${download}\n")
    endif()
    string(REPLACE "\n" "\n                " indented "${copyright}")
    string(APPEND body "    著作権:     ${indented}\n\n")

    foreach(id IN LISTS ids)
        if(NOT EXISTS "${spdx_dir}/${id}.txt")
            list(APPEND missing "${id} (${name} が必要とする)")
        else()
            list(APPEND needed_ids "${id}")
        endif()
    endforeach()
endforeach()

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
message(STATUS "third_party_licenses.txt を書き出した: "
               "第三者 ${shipped_count} 件 / 法文 ${id_count} 種")
