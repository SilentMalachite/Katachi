# 不変条件スキャナ（docs/phases.md §3 の 6 種）
#
# CLAUDE.md の「絶対禁止」を機械化したもの。人手のレビューに頼らない。
#
# 使い方:
#   cmake -DKATACHI_SCAN_CHECK=<CHECK> -DKATACHI_SCAN_ROOT=<src 相当のディレクトリ> \
#         -P tests/invariants/scan_invariants.cmake
#
# 違反が 1 件でもあれば非ゼロ終了する。
#
# CHECK の一覧:
#   INV1  std::enable_if / std::void_t / SFINAE の痕跡
#   INV2  無制約テンプレート（concept 定義・requires 節付きを除く）
#   INV3A <ROOT>/core の文字列リテラル中のフォーマット名（FormatId.hpp を除く）
#   INV3B <ROOT>/app の文字列リテラル中のフォーマット名
#
# INV3A は当初 core の文字列リテラルを全面禁止していたが、
# docs/spec-core.md §3 の原文は「フォーマット名の文字列リテラルを書かない」であり、
# 全面禁止はそこに無い強化だった。NamingRule の予約語（{name} / {ext} / {index}）という
# フォーマット名でないリテラルを阻んだため、承認を得て仕様どおりの判定へ戻した。
#   INV4  <ROOT>/core からの io/ ・ QtWidgets の include
#   INV5  QtNetwork / QNetwork* の include
#   INV6  NOLINT / 警告抑制プラグマ

cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED KATACHI_SCAN_CHECK)
    message(FATAL_ERROR "KATACHI_SCAN_CHECK is required")
endif()
if(NOT DEFINED KATACHI_SCAN_ROOT)
    message(FATAL_ERROR "KATACHI_SCAN_ROOT is required")
endif()
# 以降 file(RELATIVE_PATH) を使うため絶対パスへ正規化する。
file(REAL_PATH "${KATACHI_SCAN_ROOT}" KATACHI_SCAN_ROOT BASE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

if(NOT IS_DIRECTORY "${KATACHI_SCAN_ROOT}")
    # ルートの指定ミスで「違反 0 件」と誤報告するのを防ぐ。
    message(FATAL_ERROR "KATACHI_SCAN_ROOT is not a directory: ${KATACHI_SCAN_ROOT}")
endif()

# src/app に書いてはならないフォーマット名。
# この一覧は tests/ 配下（スキャナ自身）にあるため、
# 「src/core・src/app にフォーマット名を書かない」という不変条件には抵触しない。
# 英単語と紛れるもの（raw 等）は誤検出を避けるため入れていない。
set(KATACHI_FORMAT_NAMES
    png jpg jpeg jpe jfif bmp dib gif tif tiff webp heic heif avif jxl jp2 j2k jpf jpx ico icns
    cur tga targa ppm pgm pbm pnm xbm xpm svg svgz psd psb dds mng wbmp pdf)

# 走査対象のサブディレクトリと、コメントを除去するか否かは検査ごとに異なる。
# INV6 は NOLINT がコメントとして書かれるため、コメントを除去してはならない。
set(scan_subdir "")
set(strip_comments TRUE)

if(KATACHI_SCAN_CHECK STREQUAL "INV3A" OR KATACHI_SCAN_CHECK STREQUAL "INV4")
    set(scan_subdir "core")
elseif(KATACHI_SCAN_CHECK STREQUAL "INV3B")
    set(scan_subdir "app")
elseif(KATACHI_SCAN_CHECK STREQUAL "INV6")
    set(strip_comments FALSE)
elseif(NOT KATACHI_SCAN_CHECK MATCHES "^INV[1256]$")
    message(FATAL_ERROR "unknown KATACHI_SCAN_CHECK: ${KATACHI_SCAN_CHECK}")
endif()

set(scan_dir "${KATACHI_SCAN_ROOT}")
if(NOT scan_subdir STREQUAL "")
    set(scan_dir "${KATACHI_SCAN_ROOT}/${scan_subdir}")
endif()

set(files "")
if(IS_DIRECTORY "${scan_dir}")
    file(GLOB_RECURSE files "${scan_dir}/*.cpp" "${scan_dir}/*.hpp")
endif()

# ファイル内容を行のリストへ読み込む。
# CMake のリスト区切り（;）とエスケープ（\）を退避しないと行が壊れるため、
# 退避文字へ置換してから改行で分割する。表示前に katachi_restore で戻す。
function(katachi_read_lines out_var path do_strip)
    file(READ "${path}" content)
    if(do_strip)
        string(REGEX REPLACE "/\\*[^*]*\\*+([^/*][^*]*\\*+)*/" "" content "${content}")
        string(REGEX REPLACE "//[^\n]*" "" content "${content}")
    endif()
    string(REPLACE "\\" "@KBS@" content "${content}")
    string(REPLACE ";" "@KSEMI@" content "${content}")
    string(REPLACE "\r" "" content "${content}")
    string(REPLACE "\n" ";" content "${content}")
    set(${out_var} "${content}" PARENT_SCOPE)
endfunction()

function(katachi_restore out_var text)
    string(REPLACE "@KSEMI@" ";" text "${text}")
    string(REPLACE "@KBS@" "\\" text "${text}")
    set(${out_var} "${text}" PARENT_SCOPE)
endfunction()

set(violations "")

foreach(file IN LISTS files)
    file(RELATIVE_PATH rel "${KATACHI_SCAN_ROOT}" "${file}")

    # docs/spec-core.md §3: フォーマット名の文字列リテラルの唯一の例外は
    # src/core/FormatId.hpp 内の変換関数。
    if(KATACHI_SCAN_CHECK STREQUAL "INV3A" AND rel MATCHES "(^|/)FormatId\\.hpp$")
        continue()
    endif()

    katachi_read_lines(lines "${file}" ${strip_comments})
    list(LENGTH lines line_count)
    if(line_count EQUAL 0)
        continue()
    endif()
    math(EXPR last_index "${line_count} - 1")

    foreach(i RANGE 0 ${last_index})
        list(GET lines ${i} line)
        math(EXPR line_no "${i} + 1")
        set(hit "")

        if(KATACHI_SCAN_CHECK STREQUAL "INV1")
            if(line MATCHES "enable_if" OR line MATCHES "void_t")
                set(hit "SFINAE の痕跡（cpp-conventions.md §1 で禁止）")
            endif()

        elseif(KATACHI_SCAN_CHECK STREQUAL "INV2")
            if(line MATCHES "template[ \t]*<[ \t]*(typename|class)[^A-Za-z0-9_]")
                # concept 定義そのもの、および requires 節で制約されたものは許可する。
                # 宣言が複数行に折り返される場合に備えて数行先まで見る。
                set(constrained FALSE)
                math(EXPR look_end "${i} + 4")
                if(look_end GREATER ${last_index})
                    set(look_end ${last_index})
                endif()
                foreach(j RANGE ${i} ${look_end})
                    list(GET lines ${j} ahead)
                    if(ahead MATCHES "concept[ \t]" OR ahead MATCHES "requires")
                        set(constrained TRUE)
                        break()
                    endif()
                endforeach()
                if(NOT constrained)
                    set(hit "無制約テンプレート（cpp-conventions.md §2 で禁止）")
                endif()
            endif()

        elseif(KATACHI_SCAN_CHECK STREQUAL "INV3A" OR KATACHI_SCAN_CHECK STREQUAL "INV3B")
            string(REGEX MATCHALL "\"[^\"]*\"" literals "${line}")
            foreach(literal IN LISTS literals)
                string(TOLOWER "${literal}" literal_lower)
                string(REGEX MATCHALL "[a-z0-9]+" tokens "${literal_lower}")
                foreach(token IN LISTS tokens)
                    list(FIND KATACHI_FORMAT_NAMES "${token}" found)
                    if(NOT found EQUAL -1)
                        set(hit "フォーマット名 '${token}' の文字列リテラル（CapabilityTable 経由にする）")
                        break()
                    endif()
                endforeach()
                if(NOT hit STREQUAL "")
                    break()
                endif()
            endforeach()

        elseif(KATACHI_SCAN_CHECK STREQUAL "INV4")
            if(line MATCHES "^[ \t]*#[ \t]*include")
                if(line MATCHES "io/" OR line MATCHES "QtWidgets")
                    set(hit "core → io / QtWidgets への依存（依存方向は core → io → app）")
                endif()
            endif()

        elseif(KATACHI_SCAN_CHECK STREQUAL "INV5")
            if(line MATCHES "^[ \t]*#[ \t]*include")
                if(line MATCHES "QtNetwork" OR line MATCHES "QNetwork")
                    set(hit "ネットワーク API の include（アプリ実行時の通信は全面禁止）")
                endif()
            endif()

        elseif(KATACHI_SCAN_CHECK STREQUAL "INV6")
            if(line MATCHES "NOLINT"
               OR line MATCHES "#[ \t]*pragma[ \t]+(GCC|clang)[ \t]+diagnostic"
               OR line MATCHES "#[ \t]*pragma[ \t]+warning")
                set(hit "警告抑制（CLAUDE.md で禁止。必要と判断したら停止して報告する）")
            endif()
        endif()

        if(NOT hit STREQUAL "")
            # 退避文字はここでは戻さない。戻すと ; がリスト区切りとして再解釈され、
            # 報告文が分割されてしまう。最後に結合してからまとめて戻す。
            string(STRIP "${line}" shown)
            list(APPEND violations "  ${rel}:${line_no}: ${hit}\n    > ${shown}")
        endif()
    endforeach()
endforeach()

list(LENGTH violations violation_count)
if(NOT violation_count EQUAL 0)
    list(JOIN violations "\n" report)
    katachi_restore(report "${report}")
    message(FATAL_ERROR "[${KATACHI_SCAN_CHECK}] 不変条件違反 ${violation_count} 件\n${report}")
endif()

message(STATUS "[${KATACHI_SCAN_CHECK}] ok (${scan_dir})")
