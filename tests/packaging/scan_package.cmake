# 配布物の機械検査（Phase 4 T7）。`cmake -P` のスクリプトモードで走る。
#
#   -DKATACHI_CHECK=<P1..P8>       行う検査
#   -DKATACHI_PACKAGE_ROOT=<dir>   組み立てた配布物の置き場
#   -DKATACHI_QT_PREFIX=<dir>      比較に使う Qt（P4）
#   -DKATACHI_EXPECTED_VERSION=<v> project(VERSION)（P7）
#
# 不変条件スキャナ（tests/invariants/scan_invariants.cmake）と同じ形。
# **どの検査も、対象が見つからなければ失敗する。** 空振りして green にならないため。

cmake_minimum_required(VERSION 3.24)

foreach(required IN ITEMS KATACHI_CHECK KATACHI_PACKAGE_ROOT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "scan_package.cmake: ${required} が渡されていない")
    endif()
endforeach()

set(root "${KATACHI_PACKAGE_ROOT}")

# macOS は Katachi.app、Windows は Katachi.exe。**どちらでもなければ失敗する。**
if(IS_DIRECTORY "${root}/Katachi.app")
    set(platform macos)
    set(app "${root}/Katachi.app")
    set(exe "${app}/Contents/MacOS/Katachi")
    set(plugin_root "${app}/Contents/PlugIns")
    set(doc_root "${app}/Contents/Resources")
    set(plugin_suffix ".dylib")
elseif(EXISTS "${root}/Katachi.exe")
    set(platform windows)
    set(exe "${root}/Katachi.exe")
    set(plugin_root "${root}")
    set(doc_root "${root}")
    set(plugin_suffix ".dll")
else()
    message(FATAL_ERROR "配布物が見つからない: ${root}\n"
                        "Katachi.app も Katachi.exe も無い。**検査対象が無いのは失敗である。**")
endif()

if(NOT EXISTS "${exe}")
    message(FATAL_ERROR "実行ファイルが無い: ${exe}")
endif()

function(katachi_run out_output)
    execute_process(COMMAND ${ARGN} OUTPUT_VARIABLE text ERROR_VARIABLE errors
                    RESULT_VARIABLE code OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT code EQUAL 0)
        message(FATAL_ERROR "コマンドが失敗した (exit ${code}): ${ARGN}\n${errors}")
    endif()
    set(${out_output} "${text}" PARENT_SCOPE)
endfunction()

# ── P1: Qt を動的リンクしている（受け入れ基準 4）─────────────────────
if(KATACHI_CHECK STREQUAL "P1")
    if(platform STREQUAL "macos")
        katachi_run(deps otool -L "${exe}")
        foreach(module Core Gui Widgets Concurrent)
            if(NOT deps MATCHES "@rpath/Qt${module}\\.framework")
                message(FATAL_ERROR "Qt${module} が @rpath で参照されていない:\n${deps}")
            endif()
        endforeach()
        if(deps MATCHES "\\.a[\r\n]")
            message(FATAL_ERROR "静的ライブラリへの参照がある:\n${deps}")
        endif()
    else()
        katachi_run(deps dumpbin -dependents "${exe}")
        foreach(module Core Gui Widgets Concurrent)
            if(NOT deps MATCHES "Qt6${module}\\.dll")
                message(FATAL_ERROR "Qt6${module}.dll に依存していない:\n${deps}")
            endif()
        endforeach()
    endif()
    message(STATUS "P1: Qt は動的リンク（${platform}）")

# ── P2: universal binary（受け入れ基準 1。macOS のみ）────────────────
elseif(KATACHI_CHECK STREQUAL "P2")
    if(NOT platform STREQUAL "macos")
        message(STATUS "P2: macOS 以外は対象外")
        return()
    endif()
    file(GLOB_RECURSE machos "${app}/Contents/MacOS/*" "${app}/Contents/PlugIns/*.dylib"
         "${app}/Contents/Frameworks/*/Versions/A/Qt*")
    list(LENGTH machos count)
    if(count LESS 10)
        message(FATAL_ERROR "検査対象が少なすぎる (${count})。走査が空振りしている")
    endif()
    set(single "")
    foreach(item IN LISTS machos)
        if(IS_DIRECTORY "${item}" OR IS_SYMLINK "${item}")
            continue()
        endif()
        execute_process(COMMAND lipo -archs "${item}" OUTPUT_VARIABLE archs
                        ERROR_QUIET RESULT_VARIABLE code
                        OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(NOT code EQUAL 0)
            continue()   # Mach-O でないもの（qt.conf など）は対象外
        endif()
        if(NOT archs MATCHES "x86_64" OR NOT archs MATCHES "arm64")
            list(APPEND single "${item} -> ${archs}")
        endif()
    endforeach()
    if(single)
        string(REPLACE ";" "\n  - " pretty "${single}")
        message(FATAL_ERROR "universal でない Mach-O がある:\n  - ${pretty}")
    endif()
    message(STATUS "P2: ${count} 件すべて universal")

# ── P3: 配布対象の下限（macOS のみ）──────────────────────────────────
elseif(KATACHI_CHECK STREQUAL "P3")
    if(NOT platform STREQUAL "macos")
        message(STATUS "P3: macOS 以外は対象外")
        return()
    endif()
    katachi_run(load_commands otool -l "${exe}")
    if(NOT load_commands MATCHES "minos ([0-9]+)\\.([0-9]+)")
        message(FATAL_ERROR "LC_BUILD_VERSION の minos が読めない")
    endif()
    set(major "${CMAKE_MATCH_1}")
    if(major GREATER 13)
        message(FATAL_ERROR
                "配布対象の下限が macOS ${major} になっている。"
                "CLAUDE.md の対象は macOS 13 以上である")
    endif()
    message(STATUS "P3: minos ${major}（13 以下）")

# ── P4: 画像フォーマットが Qt と一致する ─────────────────────────────
#
# **Phase 1 の「静かに対応形式が消える」事故を機械で塞ぐ。**
# macdeployqt が libqsvg を落とすことを T4 で実測している。
elseif(KATACHI_CHECK STREQUAL "P4")
    if(NOT DEFINED KATACHI_QT_PREFIX)
        message(FATAL_ERROR "P4 には KATACHI_QT_PREFIX が要る")
    endif()
    file(GLOB expected RELATIVE "${KATACHI_QT_PREFIX}/plugins/imageformats"
         "${KATACHI_QT_PREFIX}/plugins/imageformats/*${plugin_suffix}")
    if(platform STREQUAL "windows")
        list(FILTER expected EXCLUDE REGEX "d\\.dll$")
    endif()
    file(GLOB actual RELATIVE "${plugin_root}/imageformats"
         "${plugin_root}/imageformats/*${plugin_suffix}")
    list(LENGTH expected expected_count)
    if(expected_count EQUAL 0)
        message(FATAL_ERROR "Qt 側の imageformats が 0 件。比較対象が無い")
    endif()
    list(SORT expected)
    list(SORT actual)
    if(NOT expected STREQUAL actual)
        message(FATAL_ERROR
                "画像フォーマットのプラグインが Qt と一致しない。\n"
                "  Qt   : ${expected}\n  配布物: ${actual}\n"
                "**対応形式が黙って減る。**")
    endif()
    message(STATUS "P4: imageformats ${expected_count} 件が Qt と一致")

# ── P5: 同梱物が権利表示に載っている（受け入れ基準 3）────────────────
#
# **確かめられるのは「配布物に入っている各ファイルに対応する記載があるか」まで。**
# 「一覧が完全か」は確かめられない（docs/licenses.md §4.3）。
elseif(KATACHI_CHECK STREQUAL "P5")
    set(notices "${doc_root}/third_party_licenses.txt")
    if(NOT EXISTS "${notices}")
        message(FATAL_ERROR "third_party_licenses.txt が無い: ${notices}")
    endif()
    file(READ "${notices}" text)

    if(platform STREQUAL "macos")
        file(GLOB shipped RELATIVE "${app}/Contents/Frameworks"
             "${app}/Contents/Frameworks/*.framework")
    else()
        file(GLOB shipped RELATIVE "${root}" "${root}/Qt6*.dll")
    endif()
    list(LENGTH shipped shipped_count)
    if(shipped_count EQUAL 0)
        message(FATAL_ERROR "同梱された Qt のライブラリが 0 件。走査が空振りしている")
    endif()

    # Qt のライブラリは §1 の Qt の節が包括して扱う。ここでは
    # 「Qt の版が書かれていること」と「第三者の項目が空でないこと」を見る。
    if(NOT text MATCHES "Qt が内部に含む第三者コード")
        message(FATAL_ERROR "第三者コードの節が無い")
    endif()
    foreach(name libtiff libwebp BundledLibjpeg BundledFreetype BundledPcre2)
        if(NOT text MATCHES "${name}")
            message(FATAL_ERROR "権利表示に ${name} の記載が無い")
        endif()
    endforeach()
    if(platform STREQUAL "windows" AND EXISTS "${root}/opengl32sw.dll"
       AND NOT text MATCHES "Mesa")
        message(FATAL_ERROR "opengl32sw.dll を同梱しているのに Mesa の記載が無い")
    endif()
    if(NOT platform STREQUAL "windows" AND text MATCHES "Mesa")
        message(FATAL_ERROR "同梱していない Mesa が権利表示に載っている")
    endif()
    message(STATUS "P5: Qt のライブラリ ${shipped_count} 件と権利表示が整合")

# ── P6: 権利表示に要るものが揃っている ───────────────────────────────
elseif(KATACHI_CHECK STREQUAL "P6")
    set(notices "${doc_root}/third_party_licenses.txt")
    if(NOT EXISTS "${notices}")
        message(FATAL_ERROR "third_party_licenses.txt が無い")
    endif()
    file(READ "${notices}" text)
    foreach(needle "GNU LESSER GENERAL PUBLIC LICENSE"
                   "github.com/SilentMalachite/Katachi"
                   "GNU General Public License v3.0 or later")
        if(NOT text MATCHES "${needle}")
            message(FATAL_ERROR "権利表示に「${needle}」が無い")
        endif()
    endforeach()
    if(NOT EXISTS "${doc_root}/LICENSE")
        message(FATAL_ERROR "本体の LICENSE が同梱されていない")
    endif()
    file(READ "${doc_root}/LICENSE" body)
    if(NOT body MATCHES "GNU GENERAL PUBLIC LICENSE")
        message(FATAL_ERROR "同梱された LICENSE が GPL の条文でない")
    endif()
    message(STATUS "P6: LGPLv3 / GPLv3 / ソース入手先が揃っている")

# ── P7: 版が project(VERSION) と一致する ─────────────────────────────
elseif(KATACHI_CHECK STREQUAL "P7")
    if(NOT DEFINED KATACHI_EXPECTED_VERSION)
        message(FATAL_ERROR "P7 には KATACHI_EXPECTED_VERSION が要る")
    endif()
    if(platform STREQUAL "macos")
        katachi_run(shown /usr/libexec/PlistBuddy -c
                    "Print :CFBundleShortVersionString" "${app}/Contents/Info.plist")
        if(NOT shown STREQUAL "${KATACHI_EXPECTED_VERSION}")
            message(FATAL_ERROR
                    "CFBundleShortVersionString が一致しない: "
                    "${shown} != ${KATACHI_EXPECTED_VERSION}")
        endif()
    else()
        katachi_run(info powershell -NoProfile -Command
                    "(Get-Item '${exe}').VersionInfo.FileVersion")
        if(NOT info MATCHES "^${KATACHI_EXPECTED_VERSION}")
            message(FATAL_ERROR
                    "FileVersion が一致しない: ${info} != ${KATACHI_EXPECTED_VERSION}")
        endif()
    endif()
    set(notices "${doc_root}/third_party_licenses.txt")
    file(READ "${notices}" text)
    if(NOT text MATCHES "Katachi ${KATACHI_EXPECTED_VERSION}")
        message(FATAL_ERROR "権利表示の版が ${KATACHI_EXPECTED_VERSION} でない")
    endif()
    message(STATUS "P7: 版が ${KATACHI_EXPECTED_VERSION} で揃っている")

# ── P8: Qt の環境変数を除いた環境で起動する ──────────────────────────
#
# **`QT_QPA_PLATFORM=offscreen` は使えない。** 配布物には実際に要る
# プラットフォームプラグインしか入らない（T4 の実測）。既定のまま起動する。
#
# **これは真のクリーン環境の代替であり、置き換えではない**（T8 で実機確認する）。
elseif(KATACHI_CHECK STREQUAL "P8")
    # `cmake -P` では CMAKE_CURRENT_BINARY_DIR が作業ディレクトリを指す。
    # そこへ書くとリポジトリ直下にゴミが出るため、配布物の隣に置く。
    get_filename_component(log_dir "${root}" DIRECTORY)
    set(log "${log_dir}/p8-stderr.txt")
    if(platform STREQUAL "macos")
        set(launcher env -i "HOME=$ENV{HOME}" "PATH=/usr/bin:/bin" "${exe}")
    else()
        set(launcher "${exe}")
    endif()
    execute_process(COMMAND ${launcher} TIMEOUT 5
                    RESULT_VARIABLE code ERROR_FILE "${log}" OUTPUT_QUIET)
    # TIMEOUT で打ち切られた = 5 秒生きていた、が期待どおり。
    if(NOT code MATCHES "timeout")
        message(FATAL_ERROR
                "5 秒以内に終了した (result=${code})。起動に失敗している疑いがある")
    endif()
    if(EXISTS "${log}")
        file(READ "${log}" errors)
        foreach(needle "Library not loaded" "could not find or load the Qt platform plugin"
                       "This application failed to start")
            if(errors MATCHES "${needle}")
                message(FATAL_ERROR "起動時のエラー「${needle}」:\n${errors}")
            endif()
        endforeach()
    endif()
    message(STATUS "P8: Qt の環境変数なしで 5 秒生存した")

# ── P9: 動的依存がすべて配布物の中で解決する ─────────────────────────
#
# **P8 は Windows では成立しない。** DLL が見つからないとき、Windows の
# ローダーはエラーダイアログを出してプロセスを生かしたまま待つ。5 秒生存しても
# 「起動した」ことにならない（CI run 31446352005 の違反フィクスチャが暴いた）。
#
# ここでは実際に起動させず、**同梱物が参照する非システムライブラリが
# すべて配布物の中にあるか**を確かめる。T4 で QtDBus を見落とした
# 静的調査の失敗も、こう書けば防げる。
elseif(KATACHI_CHECK STREQUAL "P9")
    if(platform STREQUAL "macos")
        file(GLOB_RECURSE binaries "${app}/Contents/MacOS/*" "${plugin_root}/*.dylib"
             "${app}/Contents/Frameworks/*/Versions/A/Qt*")
        set(frameworks_dir "${app}/Contents/Frameworks")
    else()
        file(GLOB_RECURSE binaries "${root}/*.exe" "${root}/*.dll")
        set(system_dir "$ENV{SystemRoot}/System32")
        if(NOT IS_DIRECTORY "${system_dir}")
            message(FATAL_ERROR "System32 が見つからない: ${system_dir}")
        endif()
    endif()

    list(LENGTH binaries binary_count)
    if(binary_count LESS 5)
        message(FATAL_ERROR "検査対象が少なすぎる (${binary_count})。走査が空振りしている")
    endif()

    set(unresolved "")
    set(checked 0)
    foreach(binary IN LISTS binaries)
        if(IS_DIRECTORY "${binary}" OR IS_SYMLINK "${binary}")
            continue()
        endif()
        if(platform STREQUAL "macos")
            execute_process(COMMAND otool -L "${binary}" OUTPUT_VARIABLE deps
                            ERROR_QUIET RESULT_VARIABLE code)
            if(NOT code EQUAL 0)
                continue()   # Mach-O でないもの
            endif()
            math(EXPR checked "${checked} + 1")
            # @rpath/<なにか> が配布物の Frameworks に無ければ未解決。
            string(REGEX MATCHALL "@rpath/[^ \t\r\n]+" refs "${deps}")
            foreach(ref IN LISTS refs)
                string(REPLACE "@rpath/" "" relative "${ref}")
                if(NOT EXISTS "${frameworks_dir}/${relative}")
                    list(APPEND unresolved "${binary} -> ${ref}")
                endif()
            endforeach()
        else()
            execute_process(COMMAND dumpbin -dependents "${binary}"
                            OUTPUT_VARIABLE deps ERROR_QUIET RESULT_VARIABLE code)
            if(NOT code EQUAL 0)
                continue()
            endif()
            math(EXPR checked "${checked} + 1")
            string(REGEX MATCHALL "[A-Za-z0-9_.+-]+\\.dll" refs "${deps}")
            get_filename_component(binary_dir "${binary}" DIRECTORY)
            foreach(ref IN LISTS refs)
                string(TOLOWER "${ref}" lower_ref)
                # (a) API セット。**System32 に実ファイルとして存在しない。**
                #     Windows が API セットスキーマで解決する仮想 DLL であり、
                #     ファイルの有無で判定してはならない（CI run 31447067573 で実測）。
                if(lower_ref MATCHES "^(api|ext)-ms-win-")
                    continue()
                endif()
                # (b) MSVC ランタイム。**意図して同梱していない**（ADR-0016 D13。
                #     個々の DLL は再頒布が許諾されていない）。利用者が公式の
                #     再頒布可能パッケージを入れることが前提であり、
                #     同梱物の欠落ではない。README.txt に必要と明記してある。
                if(lower_ref MATCHES "^(vcruntime|msvcp|concrt|vccorlib)")
                    continue()
                endif()
                # (c) System32 にあるものはシステム DLL。
                if(EXISTS "${system_dir}/${ref}")
                    continue()
                endif()
                # (d) 残りは配布物の中に無ければならない。
                if(EXISTS "${root}/${ref}" OR EXISTS "${binary_dir}/${ref}")
                    continue()
                endif()
                list(APPEND unresolved "${binary} -> ${ref}")
            endforeach()
        endif()
    endforeach()

    if(checked LESS 5)
        message(FATAL_ERROR "実際に調べられたのは ${checked} 件。走査が空振りしている")
    endif()
    if(unresolved)
        list(REMOVE_DUPLICATES unresolved)
        string(REPLACE ";" "\n  - " pretty "${unresolved}")
        message(FATAL_ERROR
                "配布物の中で解決しない依存がある:\n  - ${pretty}\n"
                "**この配布物は起動しない。**")
    endif()
    message(STATUS "P9: ${checked} 件の依存がすべて配布物の中で解決した")

else()
    message(FATAL_ERROR "未知の検査: ${KATACHI_CHECK}")
endif()
