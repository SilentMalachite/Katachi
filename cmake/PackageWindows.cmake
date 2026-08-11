# Windows の配布物を組み立てる（ADR-0015 D5 / Phase 4 T6）。
#
# `cmake -P` のスクリプトモードで走る。要る変数:
#   KATACHI_STAGE_DIR     cmake --install の接頭辞。Katachi.exe がある
#   KATACHI_SOURCE_DIR    リポジトリの root
#   KATACHI_QT_PREFIX     Qt のインストール接頭辞
#   KATACHI_APP_VERSION / KATACHI_QT_VERSION
# 任意:
#   KATACHI_OUTPUT_ZIP    ポータブル zip の出力先
#   KATACHI_ISCC          Inno Setup のコンパイラ (ISCC.exe)。渡すとインストーラも作る
#
# **コード署名は行わない**（ADR-0015 論点 5。証明書を持たない）。
# SmartScreen の警告が出ることを README.md に明記してある。

cmake_minimum_required(VERSION 3.24)

foreach(required IN ITEMS KATACHI_STAGE_DIR KATACHI_SOURCE_DIR KATACHI_QT_PREFIX
                          KATACHI_APP_VERSION KATACHI_QT_VERSION)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "PackageWindows.cmake: ${required} が渡されていない")
    endif()
endforeach()

set(exe "${KATACHI_STAGE_DIR}/Katachi.exe")
set(qt_plugins "${KATACHI_QT_PREFIX}/plugins")

if(NOT EXISTS "${exe}")
    message(FATAL_ERROR "据え付けた ${exe} が無い。先に cmake --install すること。")
endif()

# ── 1. windeployqt ──────────────────────────────────────────────────
find_program(windeployqt NAMES windeployqt HINTS "${KATACHI_QT_PREFIX}/bin"
             NO_DEFAULT_PATH)
if(NOT windeployqt)
    message(FATAL_ERROR "windeployqt が ${KATACHI_QT_PREFIX}/bin に無い")
endif()

# 渡す指定と、その理由。
#   --release              Release ビルドを対象にする（デバッグ版 DLL を入れさせない）
#   --no-translations      本アプリは翻訳を持たない（T1 の実測で 31 個入っていた）
#   --no-compiler-runtime  **MSVC ランタイムは再頒布が許諾されていない**（ADR-0016 D13）。
#                          Qt 公式が「公式の Microsoft 再頒布インストーラのみを使え」と
#                          明示している。インストーラ側で公式版を実行し、
#                          zip 側は README で要求する
#   --no-system-d3d-compiler / --no-system-dxc-compiler
#                          これらはビルド機の Windows SDK 由来であり、
#                          MSVC ランタイムと同じ範疇の懸念がある。入れない
#
# **opengl32sw.dll は残す**（--no-opengl-sw を渡さない）。GPU ドライバの無い環境
# （VM / RDP）で Qt が使う代替経路であり、**その環境を検証できない以上、
# 外す判断の根拠が無い。** 権利表示は packaging/licenses/extra-components.json で扱う。
message(STATUS "windeployqt を実行: ${exe}")
execute_process(
    COMMAND "${windeployqt}" --release --no-translations --no-compiler-runtime
            --no-system-d3d-compiler --no-system-dxc-compiler "${exe}"
    RESULT_VARIABLE deploy_result OUTPUT_QUIET)
if(NOT deploy_result EQUAL 0)
    message(FATAL_ERROR "windeployqt が失敗した (exit ${deploy_result})")
endif()

# ── 2. 使わないものを削る（ADR-0015 D12）────────────────────────────
#
# Windows では generic/qtuiotouchplugin が Qt6Network を引く（T1 の実測）。
# **README.md は「アプリはネットワーク通信を一切行わない」と明記している。**
# 配布物の中身をその記述と食い違わせない。
set(katachi_drop_plugin_dirs generic networkinformation tls platforminputcontexts)
set(katachi_drop_dlls
    Qt6Network Qt6OpenGL Qt6Qml Qt6QmlMeta Qt6QmlModels Qt6QmlWorkerScript
    Qt6Quick Qt6VirtualKeyboard Qt6VirtualKeyboardQml)

foreach(dir IN LISTS katachi_drop_plugin_dirs)
    file(REMOVE_RECURSE "${KATACHI_STAGE_DIR}/${dir}")
endforeach()
foreach(dll IN LISTS katachi_drop_dlls)
    file(REMOVE "${KATACHI_STAGE_DIR}/${dll}.dll")
endforeach()

# ── 3. 画像フォーマットのプラグインが Qt と一致するか ───────────────
#
# macOS では macdeployqt が libqsvg を落とすことを実測した（T4）。
# **Windows でも同じことが起きないと決めつけない。** 欠けていれば戻す。
file(GLOB qt_imageformats RELATIVE "${qt_plugins}/imageformats"
     "${qt_plugins}/imageformats/*.dll")
list(FILTER qt_imageformats EXCLUDE REGEX "d\\.dll$")   # デバッグ版を除く
file(GLOB bundled_imageformats RELATIVE "${KATACHI_STAGE_DIR}/imageformats"
     "${KATACHI_STAGE_DIR}/imageformats/*.dll")

foreach(plugin IN LISTS qt_imageformats)
    if(NOT plugin IN_LIST bundled_imageformats)
        message(STATUS "windeployqt が落とした画像プラグインを戻す: ${plugin}")
        file(COPY "${qt_plugins}/imageformats/${plugin}"
             DESTINATION "${KATACHI_STAGE_DIR}/imageformats")
    endif()
endforeach()

# ── 4. ライセンスを同梱する（受け入れ基準 3）────────────────────────
execute_process(
    COMMAND "${CMAKE_COMMAND}"
            -DKATACHI_LICENSE_DIR=${KATACHI_SOURCE_DIR}/packaging/licenses
            -DKATACHI_OUTPUT=${KATACHI_STAGE_DIR}/third_party_licenses.txt
            -DKATACHI_APP_VERSION=${KATACHI_APP_VERSION}
            -DKATACHI_QT_VERSION=${KATACHI_QT_VERSION}
            -DKATACHI_PLATFORM=windows
            -P "${KATACHI_SOURCE_DIR}/cmake/ThirdPartyLicenses.cmake"
    RESULT_VARIABLE licenses_result)
if(NOT licenses_result EQUAL 0)
    message(FATAL_ERROR "third_party_licenses.txt を作れなかった")
endif()

file(COPY "${KATACHI_SOURCE_DIR}/LICENSE" DESTINATION "${KATACHI_STAGE_DIR}")

# **ポータブル zip は MSVC 再頒布可能パッケージを要求する**（D13）。
# 「入っていないのに動くはず」と書かない。同梱の説明に必要と書く。
file(WRITE "${KATACHI_STAGE_DIR}/README.txt"
"Katachi ${KATACHI_APP_VERSION} — ポータブル版（Windows x64）

Katachi.exe をそのまま実行してください。インストールは不要です。

■ 事前に必要なもの

  Microsoft Visual C++ 再頒布可能パッケージ (x64)

  入っていない場合は Katachi.exe が起動しません。次から入手してください。
      https://aka.ms/vs/17/release/vc_redist.x64.exe

  ※ このパッケージに含まれるランタイム DLL は、個別に再頒布することが
     許諾されていません。そのため本 zip には同梱していません。
     インストーラ版 (Katachi-${KATACHI_APP_VERSION}-setup.exe) は、
     公式の再頒布パッケージを自動で実行します。

■ 署名について

  この配布物にはコード署名がありません。初回起動時に Windows の
  SmartScreen が警告を出します。[詳細情報] から実行できます。

■ ライセンス

  本体は GNU General Public License v3.0 or later です。条文は LICENSE を、
  同梱する第三者コードの権利表示は third_party_licenses.txt を参照してください。
  対応するソースコード: https://github.com/SilentMalachite/Katachi
")

# ── 5. 検査 ─────────────────────────────────────────────────────────
foreach(dll IN LISTS katachi_drop_dlls)
    if(EXISTS "${KATACHI_STAGE_DIR}/${dll}.dll")
        message(FATAL_ERROR "削ったはずの ${dll}.dll が残っている")
    endif()
endforeach()

file(GLOB stray "${KATACHI_STAGE_DIR}/msvcp*.dll" "${KATACHI_STAGE_DIR}/vcruntime*.dll"
     "${KATACHI_STAGE_DIR}/concrt*.dll" "${KATACHI_STAGE_DIR}/vccorlib*.dll")
if(stray)
    string(REPLACE ";" "\n  - " pretty "${stray}")
    message(FATAL_ERROR
            "MSVC ランタイムが配置されている:\n  - ${pretty}\n\n"
            "**これらは再頒布が許諾されていない**（ADR-0016 D13）。"
            "--no-compiler-runtime が効いていない。")
endif()

file(GLOB bundled_imageformats RELATIVE "${KATACHI_STAGE_DIR}/imageformats"
     "${KATACHI_STAGE_DIR}/imageformats/*.dll")
list(LENGTH qt_imageformats qt_count)
list(LENGTH bundled_imageformats bundled_count)
if(NOT qt_count EQUAL bundled_count)
    message(FATAL_ERROR
            "画像フォーマットのプラグインが Qt と一致しない: "
            "Qt=${qt_count} 配布物=${bundled_count}\n**対応形式が黙って減る。**")
endif()

foreach(needed IN ITEMS Katachi.exe LICENSE third_party_licenses.txt README.txt
                        platforms/qwindows.dll)
    if(NOT EXISTS "${KATACHI_STAGE_DIR}/${needed}")
        message(FATAL_ERROR "同梱すべき ${needed} が無い")
    endif()
endforeach()

# ── 6. ポータブル zip ───────────────────────────────────────────────
if(DEFINED KATACHI_OUTPUT_ZIP AND NOT KATACHI_OUTPUT_ZIP STREQUAL "")
    file(REMOVE "${KATACHI_OUTPUT_ZIP}")
    file(GLOB_RECURSE zip_items RELATIVE "${KATACHI_STAGE_DIR}" "${KATACHI_STAGE_DIR}/*")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cf "${KATACHI_OUTPUT_ZIP}"
                            --format=zip ${zip_items}
                    WORKING_DIRECTORY "${KATACHI_STAGE_DIR}"
                    RESULT_VARIABLE zip_result)
    if(NOT zip_result EQUAL 0)
        message(FATAL_ERROR "zip の作成に失敗した")
    endif()
    message(STATUS "ポータブル zip を作った: ${KATACHI_OUTPUT_ZIP}")
endif()

# ── 7. インストーラ（Inno Setup。ADR-0015 論点 6）───────────────────
if(DEFINED KATACHI_ISCC AND NOT KATACHI_ISCC STREQUAL "")
    if(NOT DEFINED KATACHI_OUTPUT_DIR OR KATACHI_OUTPUT_DIR STREQUAL "")
        get_filename_component(KATACHI_OUTPUT_DIR "${KATACHI_STAGE_DIR}" DIRECTORY)
    endif()
    set(iss "${KATACHI_OUTPUT_DIR}/katachi.iss")
    configure_file("${KATACHI_SOURCE_DIR}/packaging/windows/katachi.iss.in" "${iss}" @ONLY)
    execute_process(COMMAND "${KATACHI_ISCC}" "${iss}"
                    RESULT_VARIABLE iscc_result OUTPUT_QUIET)
    if(NOT iscc_result EQUAL 0)
        message(FATAL_ERROR "Inno Setup が失敗した (exit ${iscc_result})")
    endif()
    message(STATUS "インストーラを作った")
endif()

message(STATUS "Windows の配布物ができた: ${KATACHI_STAGE_DIR}")
message(STATUS "  画像フォーマットのプラグイン: ${bundled_count} 個（Qt と一致）")
message(STATUS "  **未署名である。** SmartScreen の警告が出る（ADR-0015 論点 5）")
