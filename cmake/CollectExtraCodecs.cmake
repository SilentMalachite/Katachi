# 追加コーデックのプラグインを配置する（ADR-0013）。**cmake -P で実行する。**
#
# cmake/ExtraCodecs.cmake から呼ばれる。構成時ではなくビルド時に走るのは、
# kimageformats の install 先の階層が KDE 側の設定で決まり、
# 構成時には確定していないためである。**推測せず、実際に探す。**
#
# 入力（-D で渡す）
#   KATACHI_COLLECT_FROM   kimageformats の install prefix
#   KATACHI_COLLECT_TO     配置先（QT_PLUGIN_PATH の根 / imageformats）
#   KATACHI_COLLECT_NAMES  配置するプラグイン名を | で連結したもの
#   KATACHI_COLLECT_STAMP  完了印のファイル
#
# **指定した名前のものだけを配置する。** kimageformats の kimg_tga / kimg_jp2 は
# Qt 同梱の qtga / qjp2 と同じ形式を扱うため、一緒に置くと同一形式のプラグインが
# 2 つ載り、どちらが使われるかが不定になる。
#
# 名前に対応するファイルが 1 つも無ければ FATAL_ERROR で落とす。
# 「ON にしたのに黙って対応形式が減る」を作らないため
# （Phase 1 の qtimageformats の取りこぼしと同じ失敗の形）。

cmake_minimum_required(VERSION 3.24)

foreach(required IN ITEMS KATACHI_COLLECT_FROM KATACHI_COLLECT_TO KATACHI_COLLECT_NAMES
                          KATACHI_COLLECT_STAMP)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} が指定されていない")
    endif()
endforeach()

string(REPLACE "|" ";" katachi_collect_names "${KATACHI_COLLECT_NAMES}")

file(MAKE_DIRECTORY "${KATACHI_COLLECT_TO}")

set(katachi_missing)
set(katachi_placed)

foreach(name IN LISTS katachi_collect_names)
    file(GLOB_RECURSE candidates "${KATACHI_COLLECT_FROM}/*${name}*")

    set(matches)
    foreach(candidate IN LISTS candidates)
        get_filename_component(base "${candidate}" NAME)
        # 共有モジュールだけを拾う。接頭辞と拡張子は OS で変わる
        # （libkimg_psd.dylib / libkimg_psd.so / kimg_psd.dll）。
        if(base MATCHES "^(lib)?${name}\\.(so|dylib|dll)$")
            list(APPEND matches "${candidate}")
        endif()
    endforeach()

    if(matches)
        foreach(match IN LISTS matches)
            file(COPY "${match}" DESTINATION "${KATACHI_COLLECT_TO}")
            get_filename_component(base "${match}" NAME)
            list(APPEND katachi_placed "${base}")
        endforeach()
    else()
        list(APPEND katachi_missing "${name}")
    endif()
endforeach()

if(katachi_missing)
    message(
        FATAL_ERROR
            "KATACHI_EXTRA_CODECS=ON だが、次のプラグインが作られていない: ${katachi_missing}\n"
            "対応するコーデックライブラリ（libavif / libjxl / LibRaw）が\n"
            "kimageformats のビルド時に見つからなかった可能性がある。\n"
            "探索先: ${KATACHI_COLLECT_FROM}")
endif()

file(WRITE "${KATACHI_COLLECT_STAMP}" "${katachi_placed}\n")
message(STATUS "追加コーデックを配置: ${katachi_placed}")
