# 配布物を故意に壊し、機械検査が確実に落ちることを確かめる（Phase 4 T7）。
#
#   -DKATACHI_CHECK=<P1..P8>       壊す対象の検査
#   -DKATACHI_PACKAGE_ROOT=<dir>   組み立て済みの配布物（**読むだけ。壊さない**）
#   -DKATACHI_WORK_DIR=<dir>       壊した複製を置く場所
#   -DKATACHI_SCAN=<path>          scan_package.cmake
#   -DKATACHI_QT_PREFIX / -DKATACHI_EXPECTED_VERSION
#
# **このスクリプト自身が判定を反転させる。** 検査が壊れた木を通してしまったら
# ここで FATAL_ERROR にする。ctest 側の WILL_FAIL には頼らない — そのほうが
# 「検査が空振りしている」という理由をログに残せるためである。
# 不変条件スキャナの `invariant.*.detects_violation` と同じ考え方である。

cmake_minimum_required(VERSION 3.24)

foreach(required IN ITEMS KATACHI_CHECK KATACHI_PACKAGE_ROOT KATACHI_WORK_DIR KATACHI_SCAN)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "violate_package.cmake: ${required} が渡されていない")
    endif()
endforeach()

set(work "${KATACHI_WORK_DIR}/${KATACHI_CHECK}")
file(REMOVE_RECURSE "${work}")
file(MAKE_DIRECTORY "${work}")

# 複製する。file(COPY) は実行権限も保つ。
file(GLOB entries "${KATACHI_PACKAGE_ROOT}/*")
if(NOT entries)
    message(FATAL_ERROR "複製元が空: ${KATACHI_PACKAGE_ROOT}")
endif()
file(COPY ${entries} DESTINATION "${work}")

if(IS_DIRECTORY "${work}/Katachi.app")
    set(platform macos)
    set(exe "${work}/Katachi.app/Contents/MacOS/Katachi")
    set(plugins "${work}/Katachi.app/Contents/PlugIns")
    set(docs "${work}/Katachi.app/Contents/Resources")
else()
    set(platform windows)
    set(exe "${work}/Katachi.exe")
    set(plugins "${work}")
    set(docs "${work}")
endif()

# ── 検査ごとの壊し方 ────────────────────────────────────────────────
#
# **「壊した」と言えるだけの壊し方をする。** ファイルを 1 つ消して
# スキャナ冒頭の存在確認に引っかかるだけでは、検査の中身を試したことにならない。
if(KATACHI_CHECK STREQUAL "P1" OR KATACHI_CHECK STREQUAL "P3")
    # Qt にリンクしていない実行ファイルへ差し替える。P1 は「Qt が無い」で、
    # P3 は「minos が動作環境のもの（13 より新しい）」で落ちるはず。
    if(platform STREQUAL "macos")
        file(COPY /bin/echo DESTINATION "${work}")
        file(RENAME "${work}/echo" "${exe}")
    else()
        file(COPY "$ENV{SystemRoot}/System32/where.exe" DESTINATION "${work}")
        file(RENAME "${work}/where.exe" "${exe}")
    endif()

elseif(KATACHI_CHECK STREQUAL "P2")
    # universal を単一アーキにする。
    execute_process(COMMAND lipo -thin arm64 "${exe}" -output "${exe}.thin"
                    RESULT_VARIABLE code OUTPUT_QUIET ERROR_QUIET)
    if(NOT code EQUAL 0)
        execute_process(COMMAND lipo -thin x86_64 "${exe}" -output "${exe}.thin"
                        RESULT_VARIABLE code OUTPUT_QUIET ERROR_QUIET)
    endif()
    if(NOT code EQUAL 0)
        message(FATAL_ERROR "lipo -thin に失敗した。フィクスチャを作れない")
    endif()
    file(RENAME "${exe}.thin" "${exe}")

elseif(KATACHI_CHECK STREQUAL "P4")
    # 画像フォーマットのプラグインを 1 つ落とす（macdeployqt が実際にやったこと）。
    file(GLOB victims "${plugins}/imageformats/*")
    list(GET victims 0 victim)
    file(REMOVE "${victim}")

elseif(KATACHI_CHECK STREQUAL "P5")
    # 権利表示から 1 件だけ消す。**ファイルは残す**ので、存在確認では捕まらない。
    file(READ "${docs}/third_party_licenses.txt" text)
    string(REPLACE "libtiff" "REMOVED-ENTRY" text "${text}")
    file(WRITE "${docs}/third_party_licenses.txt" "${text}")

elseif(KATACHI_CHECK STREQUAL "P6")
    # 本体の LICENSE を、GPL でない中身に差し替える。
    file(WRITE "${docs}/LICENSE" "not a license\n")

elseif(KATACHI_CHECK STREQUAL "P7")
    # 版を食い違わせる。
    if(platform STREQUAL "macos")
        execute_process(COMMAND /usr/libexec/PlistBuddy -c
                        "Set :CFBundleShortVersionString 9.9.9"
                        "${work}/Katachi.app/Contents/Info.plist" OUTPUT_QUIET)
    else()
        file(READ "${docs}/third_party_licenses.txt" text)
        string(REPLACE "Katachi ${KATACHI_EXPECTED_VERSION}" "Katachi 9.9.9" text "${text}")
        file(WRITE "${docs}/third_party_licenses.txt" "${text}")
    endif()

elseif(KATACHI_CHECK STREQUAL "P8" OR KATACHI_CHECK STREQUAL "P9")
    # 起動に要るライブラリを落とす。**T4 で QtDBus を消して実際に起きた壊れ方。**
    if(platform STREQUAL "macos")
        file(REMOVE "${work}/Katachi.app/Contents/Frameworks/QtDBus.framework/Versions/A/QtDBus")
    else()
        file(REMOVE "${work}/Qt6Core.dll")
    endif()

else()
    message(FATAL_ERROR "壊し方が定義されていない検査: ${KATACHI_CHECK}")
endif()

# ── 壊した木に対して検査を走らせ、その exit をそのまま返す ──────────
execute_process(
    COMMAND "${CMAKE_COMMAND}"
            -DKATACHI_CHECK=${KATACHI_CHECK}
            -DKATACHI_PACKAGE_ROOT=${work}
            -DKATACHI_QT_PREFIX=${KATACHI_QT_PREFIX}
            -DKATACHI_EXPECTED_VERSION=${KATACHI_EXPECTED_VERSION}
            -P "${KATACHI_SCAN}"
    RESULT_VARIABLE scan_result)

if(scan_result EQUAL 0)
    message(FATAL_ERROR
            "${KATACHI_CHECK} は壊した配布物を通してしまった。**検査が空振りしている。**")
endif()
message(STATUS "${KATACHI_CHECK}: 壊した配布物を正しく拒否した (exit ${scan_result})")
