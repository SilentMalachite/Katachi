# macOS の配布物を組み立てる（ADR-0015 D5 / Phase 4 T5）。
#
# `cmake -P` のスクリプトモードで走る。構成時には確定しない情報
# （macdeployqt が実際に何を入れたか）をビルド後に実測して扱うため、
# cmake/CollectExtraCodecs.cmake と同じ形を採る。
#
# 要る変数:
#   KATACHI_STAGE_DIR     cmake --install の接頭辞。Katachi.app がある
#   KATACHI_SOURCE_DIR    リポジトリの root
#   KATACHI_QT_PREFIX     Qt のインストール接頭辞（bin/macdeployqt と plugins/ を使う）
#   KATACHI_APP_VERSION   成果物の版
#   KATACHI_QT_VERSION    Qt の版
# 任意:
#   KATACHI_SIGN_IDENTITY 指定すると codesign する（Developer ID）。docs/release.md
#
# **署名と公証はローカルでのみ行う**（ADR-0015 論点 4）。CI はここまでを作る。

cmake_minimum_required(VERSION 3.24)

foreach(required IN ITEMS KATACHI_STAGE_DIR KATACHI_SOURCE_DIR KATACHI_QT_PREFIX
                          KATACHI_APP_VERSION KATACHI_QT_VERSION)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "PackageMacOS.cmake: ${required} が渡されていない")
    endif()
endforeach()

set(app "${KATACHI_STAGE_DIR}/Katachi.app")
set(contents "${app}/Contents")
set(qt_plugins "${KATACHI_QT_PREFIX}/plugins")

if(NOT IS_DIRECTORY "${app}")
    message(FATAL_ERROR "据え付けた ${app} が無い。先に cmake --install すること。")
endif()

# ── 1. macdeployqt ──────────────────────────────────────────────────
find_program(macdeployqt NAMES macdeployqt HINTS "${KATACHI_QT_PREFIX}/bin"
             NO_DEFAULT_PATH)
if(NOT macdeployqt)
    message(FATAL_ERROR "macdeployqt が ${KATACHI_QT_PREFIX}/bin に無い")
endif()

message(STATUS "macdeployqt を実行: ${app}")
execute_process(COMMAND "${macdeployqt}" "${app}" -no-strip
                RESULT_VARIABLE deploy_result OUTPUT_QUIET)
if(NOT deploy_result EQUAL 0)
    message(FATAL_ERROR "macdeployqt が失敗した (exit ${deploy_result})")
endif()

# ── 2. 使わないものを削る（ADR-0015 D12）────────────────────────────
#
# 仮想キーボードのプラグイン 1 つが 8 フレームワークを引き込む。その中に
#   QtQuick   … ADR-0001 は「Qt Quick 不採用」と決めている
#   QtNetwork … README.md は「ネットワーク通信を一切行わない」と明記している
# が含まれる。**配布物の中身を、明文化した決定と食い違わせない。**
#
# QtDBus は削らない。QtGui が参照しており、削ると起動しない（T4 の実測）。
set(katachi_drop_plugins platforminputcontexts)
set(katachi_drop_frameworks
    QtNetwork QtOpenGL QtQml QtQmlMeta QtQmlModels QtQmlWorkerScript
    QtQuick QtVirtualKeyboard QtVirtualKeyboardQml)

foreach(kind IN LISTS katachi_drop_plugins)
    file(REMOVE_RECURSE "${contents}/PlugIns/${kind}")
endforeach()
foreach(framework IN LISTS katachi_drop_frameworks)
    file(REMOVE_RECURSE "${contents}/Frameworks/${framework}.framework")
endforeach()

# ── 3. macdeployqt が落とすものを戻す ───────────────────────────────
#
# **macdeployqt は imageformats/libqsvg を入れない**（T4 の実測）。
# 放置すると配布物だけ SVG が読めなくなる。理由は調べていないが、
# 事実として落ちるので明示的に戻す。
file(GLOB qt_imageformats RELATIVE "${qt_plugins}/imageformats"
     "${qt_plugins}/imageformats/*.dylib")
file(GLOB bundled_imageformats RELATIVE "${contents}/PlugIns/imageformats"
     "${contents}/PlugIns/imageformats/*.dylib")

foreach(plugin IN LISTS qt_imageformats)
    if(NOT plugin IN_LIST bundled_imageformats)
        message(STATUS "macdeployqt が落とした画像プラグインを戻す: ${plugin}")
        file(COPY "${qt_plugins}/imageformats/${plugin}"
             DESTINATION "${contents}/PlugIns/imageformats")
    endif()
endforeach()

# ── 4. ライセンスを同梱する（受け入れ基準 3）────────────────────────
execute_process(
    COMMAND "${CMAKE_COMMAND}"
            -DKATACHI_LICENSE_DIR=${KATACHI_SOURCE_DIR}/packaging/licenses
            -DKATACHI_OUTPUT=${contents}/Resources/third_party_licenses.txt
            -DKATACHI_APP_VERSION=${KATACHI_APP_VERSION}
            -DKATACHI_QT_VERSION=${KATACHI_QT_VERSION}
            -DKATACHI_PLATFORM=macos
            -P "${KATACHI_SOURCE_DIR}/cmake/ThirdPartyLicenses.cmake"
    RESULT_VARIABLE licenses_result)
if(NOT licenses_result EQUAL 0)
    message(FATAL_ERROR "third_party_licenses.txt を作れなかった")
endif()

# 本体のライセンス。**改変せずそのまま複製する。**
file(COPY "${KATACHI_SOURCE_DIR}/LICENSE" DESTINATION "${contents}/Resources")

# ── 5. 組み立てた結果を検査する ─────────────────────────────────────
#
# **ここで確かめるのは「作った直後の形」である。** 配布物としての性質
# （universal / 動的リンク / 起動）は tests/packaging の P1〜P8 が見る（T7）。
foreach(framework IN LISTS katachi_drop_frameworks)
    if(IS_DIRECTORY "${contents}/Frameworks/${framework}.framework")
        message(FATAL_ERROR "削ったはずの ${framework} が残っている")
    endif()
endforeach()

file(GLOB bundled_imageformats RELATIVE "${contents}/PlugIns/imageformats"
     "${contents}/PlugIns/imageformats/*.dylib")
list(LENGTH qt_imageformats qt_count)
list(LENGTH bundled_imageformats bundled_count)
if(NOT qt_count EQUAL bundled_count)
    message(FATAL_ERROR
            "画像フォーマットのプラグインが Qt と一致しない: "
            "Qt=${qt_count} バンドル=${bundled_count}\n"
            "**対応形式が黙って減る。**")
endif()

foreach(needed IN ITEMS Resources/third_party_licenses.txt Resources/LICENSE
                        Resources/katachi.icns Info.plist)
    if(NOT EXISTS "${contents}/${needed}")
        message(FATAL_ERROR "同梱すべき ${needed} が無い")
    endif()
endforeach()

# ── 6. 署名（任意。ローカルでのみ行う）──────────────────────────────
if(DEFINED KATACHI_SIGN_IDENTITY AND NOT KATACHI_SIGN_IDENTITY STREQUAL "")
    message(STATUS "署名: ${KATACHI_SIGN_IDENTITY}")
    # 内側から署名する。フレームワークとプラグインを先に済ませないと、
    # あとで .app を署名しても中身の署名が無いまま残る。
    file(GLOB_RECURSE inner "${contents}/Frameworks/*.dylib"
         "${contents}/PlugIns/*.dylib")
    file(GLOB frameworks "${contents}/Frameworks/*.framework")
    foreach(item IN LISTS inner frameworks)
        execute_process(COMMAND codesign --force --options runtime --timestamp
                                --sign "${KATACHI_SIGN_IDENTITY}" "${item}"
                        RESULT_VARIABLE sign_result OUTPUT_QUIET ERROR_QUIET)
        if(NOT sign_result EQUAL 0)
            message(FATAL_ERROR "署名に失敗: ${item}")
        endif()
    endforeach()
    execute_process(COMMAND codesign --force --options runtime --timestamp
                            --sign "${KATACHI_SIGN_IDENTITY}" "${app}"
                    RESULT_VARIABLE sign_result)
    if(NOT sign_result EQUAL 0)
        message(FATAL_ERROR "署名に失敗: ${app}")
    endif()
else()
    message(STATUS "署名しない（KATACHI_SIGN_IDENTITY が未指定）。"
                   "公証には署名が要る。docs/release.md を参照")
endif()

# ── 7. .dmg を作る（任意）──────────────────────────────────────────
if(DEFINED KATACHI_OUTPUT_DMG AND NOT KATACHI_OUTPUT_DMG STREQUAL "")
    file(REMOVE "${KATACHI_OUTPUT_DMG}")
    # hdiutil は macOS 同梱。Applications への symlink を添えて、
    # 利用者が引き込むだけで入れられるようにする。
    set(dmg_root "${KATACHI_STAGE_DIR}/.dmg-root")
    file(REMOVE_RECURSE "${dmg_root}")
    file(MAKE_DIRECTORY "${dmg_root}")
    execute_process(COMMAND cp -R "${app}" "${dmg_root}/" RESULT_VARIABLE copy_result)
    if(NOT copy_result EQUAL 0)
        message(FATAL_ERROR ".dmg 用の複製に失敗した")
    endif()
    execute_process(COMMAND ln -s /Applications "${dmg_root}/Applications")
    # ライセンスは .app の中にもあるが、開いてすぐ見えるところにも置く。
    file(COPY "${contents}/Resources/LICENSE" DESTINATION "${dmg_root}")
    file(COPY "${contents}/Resources/third_party_licenses.txt" DESTINATION "${dmg_root}")

    execute_process(
        COMMAND hdiutil create -volname "Katachi ${KATACHI_APP_VERSION}"
                -srcfolder "${dmg_root}" -ov -format UDZO "${KATACHI_OUTPUT_DMG}"
        RESULT_VARIABLE dmg_result OUTPUT_QUIET)
    if(NOT dmg_result EQUAL 0)
        message(FATAL_ERROR "hdiutil が失敗した (exit ${dmg_result})")
    endif()
    file(REMOVE_RECURSE "${dmg_root}")
    message(STATUS ".dmg を作った: ${KATACHI_OUTPUT_DMG}")

    # **.dmg 自身にも署名する。** .app の署名とは別物で、付けないと
    # 「code object is not signed at all」のまま公証へ出すことになる（実測）。
    if(DEFINED KATACHI_SIGN_IDENTITY AND NOT KATACHI_SIGN_IDENTITY STREQUAL "")
        execute_process(COMMAND codesign --force --timestamp
                                --sign "${KATACHI_SIGN_IDENTITY}" "${KATACHI_OUTPUT_DMG}"
                        RESULT_VARIABLE dmg_sign_result)
        if(NOT dmg_sign_result EQUAL 0)
            message(FATAL_ERROR ".dmg の署名に失敗した")
        endif()
        message(STATUS ".dmg に署名した。次は公証（docs/release.md §3）")
    endif()

    if(NOT DEFINED KATACHI_SIGN_IDENTITY OR KATACHI_SIGN_IDENTITY STREQUAL "")
        message(STATUS "  **この .dmg は未署名・未公証である。** 配布してはならない。"
                       "docs/release.md の手順で署名・公証すること")
    endif()
endif()

message(STATUS "macOS の配布物ができた: ${app}")
message(STATUS "  画像フォーマットのプラグイン: ${bundled_count} 個（Qt と一致）")
