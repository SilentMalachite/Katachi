# 追加コーデック（ADR-0013）。**既定 OFF。**
#
# ON のときだけ ECM と kimageformats を固定タグで別ビルドし、
# kimg_psd / kimg_raw / kimg_avif / kimg_jxl の 4 つだけを
# ${KATACHI_EXTRA_PLUGIN_ROOT}/imageformats/ へ配置する。
#
# ここで作るのは「Qt が実行時に読み込む共有ライブラリ」だけである。
# **katachi_app はこれらにリンクしない。** そのため:
#   - 追加コーデックが無い環境でもビルド・起動する（docs/phases.md §4 Phase 3 受け入れ基準 2）
#   - 能力表は実行時の問い合わせで反映するため src/ の変更が要らない（同 受け入れ基準 3）
#
# FetchContent ではなく ExternalProject_Add を使うのは、別プロセスの CMake 実行になり
# KDE 側のビルド設定（KDECMakeSettings 等）が本プロジェクトの -Werror へ
# 干渉しないためである（ADR-0013）。

if(NOT KATACHI_EXTRA_CODECS)
    return()
endif()

include(ExternalProject)

# タグは固定する。ブランチ名やレンジで追跡しない（docs/phases.md §1.5）。
#
# **v6.20.0 を選ぶ理由（ADR-0013）**: kimageformats の最新 v6.28.1 は Qt 6.9.0 を
# 要求し、CI の Qt 6.8.3 固定（docs/phases.md §1.5、Phase 0 の決定）と両立しない。
# v6.20.0 は Qt 6.8.0 要求で、必要な 4 プラグインをすべて含む。
set(KATACHI_ECM_TAG v6.20.0)
set(KATACHI_KIMAGEFORMATS_TAG v6.20.0)

set(katachi_extra_root "${CMAKE_BINARY_DIR}/extra-codecs")
set(katachi_ecm_install "${katachi_extra_root}/ecm-install")
set(katachi_kif_install "${katachi_extra_root}/kimageformats-install")

# QT_PLUGIN_PATH に渡す根。Qt は <根>/imageformats/ を見る。
set(KATACHI_EXTRA_PLUGIN_ROOT "${CMAKE_BINARY_DIR}/plugins")

# ECM は CMake モジュール集であり、コンパイルは発生しない。
# BUILD_DOC は Sphinx を要求するため切る。
ExternalProject_Add(
    katachi_ecm
    GIT_REPOSITORY https://github.com/KDE/extra-cmake-modules.git
    GIT_TAG ${KATACHI_ECM_TAG}
    GIT_SHALLOW TRUE
    PREFIX "${katachi_extra_root}/ecm"
    INSTALL_DIR "${katachi_ecm_install}"
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${katachi_ecm_install} -DBUILD_DOC=OFF -DBUILD_TESTING=OFF
    UPDATE_DISCONNECTED TRUE)

# kimageformats 本体。Qt は本プロジェクトが使っているものと同じものを渡す。
# **別の Qt に対してビルドされたプラグインは使えない**ため、ここは揃える必要がある。
set(katachi_kif_args
    -DCMAKE_INSTALL_PREFIX=${katachi_kif_install}
    -DCMAKE_PREFIX_PATH=${katachi_ecm_install}|${CMAKE_PREFIX_PATH}
    -DQt6_DIR=${Qt6_DIR}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DBUILD_TESTING=OFF
    # ADR-0013: HEIF は対象に含めない。既定でも OFF だが、決定を構成に明示しておく。
    -DKIMAGEFORMATS_HEIF=OFF)

if(APPLE AND CMAKE_OSX_SYSROOT)
    list(APPEND katachi_kif_args -DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT})
endif()

ExternalProject_Add(
    katachi_kimageformats
    DEPENDS katachi_ecm
    GIT_REPOSITORY https://github.com/KDE/kimageformats.git
    GIT_TAG ${KATACHI_KIMAGEFORMATS_TAG}
    GIT_SHALLOW TRUE
    PREFIX "${katachi_extra_root}/kimageformats"
    INSTALL_DIR "${katachi_kif_install}"
    LIST_SEPARATOR |
    CMAKE_ARGS ${katachi_kif_args}
    UPDATE_DISCONNECTED TRUE)

# kimageformats は 25 個のプラグインを作る。そのうち kimg_tga / kimg_jp2 は
# Qt 同梱の qtga / qjp2 と**同じ形式**を扱うため、両方を置くと
# どちらが使われるかが不定になる（ADR-0013）。**4 つだけを配置する。**
set(katachi_extra_stamp "${katachi_extra_root}/placed.stamp")

add_custom_command(
    OUTPUT "${katachi_extra_stamp}"
    COMMAND
        "${CMAKE_COMMAND}" -DKATACHI_COLLECT_FROM=${katachi_kif_install}
        -DKATACHI_COLLECT_TO=${KATACHI_EXTRA_PLUGIN_ROOT}/imageformats
        -DKATACHI_COLLECT_NAMES=kimg_psd|kimg_raw|kimg_avif|kimg_jxl
        -DKATACHI_COLLECT_STAMP=${katachi_extra_stamp} -P
        "${PROJECT_SOURCE_DIR}/cmake/CollectExtraCodecs.cmake"
    DEPENDS katachi_kimageformats
    COMMENT "追加コーデックのプラグインを配置"
    VERBATIM)

add_custom_target(katachi_extra_codecs ALL DEPENDS "${katachi_extra_stamp}")
