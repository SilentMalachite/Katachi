cmake_minimum_required(VERSION 3.24)

# macOS: compile_commands.json に -isysroot を含めるため SDK を明示する。
# Apple clang は SDK を暗黙に見つけるが、品質ゲートで使う Homebrew LLVM の
# clang-tidy は見つけられず、<type_traits> の解決に失敗して誤検出を出す。
# project() より前に置く必要がある（コンパイラ検出時に読まれるため）。
if(CMAKE_HOST_APPLE AND NOT DEFINED CMAKE_OSX_SYSROOT)
    execute_process(
        COMMAND xcrun --show-sdk-path
        OUTPUT_VARIABLE katachi_macos_sdk
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(katachi_macos_sdk)
        set(CMAKE_OSX_SYSROOT "${katachi_macos_sdk}" CACHE PATH "macOS SDK path")
    endif()
endif()

project(
    Katachi
    VERSION 0.1.0
    DESCRIPTION "Qt6 / C++20 cross-platform image format converter"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(KATACHI_SANITIZE "Enable AddressSanitizer + UndefinedBehaviorSanitizer" OFF)

# CLAUDE.md: Qt 6.8 LTS 以上。Qt は LGPLv3 のため動的リンクのみ（docs/licenses.md）。
# Concurrent は QtConcurrent::mapped（docs/spec-core.md §6）のために要る。
# qtbase 同梱のはずだが「入っているはず」で進めない。CI で実際に通ることを確認する
# （Phase 1 の qtimageformats の件があるため）。
find_package(Qt6 6.8 REQUIRED COMPONENTS Concurrent Core Gui Widgets)

# 警告設定は INTERFACE ターゲットで配る。FetchContent で取得する第三者コード
# （Catch2）へ -Werror を波及させないため、グローバルフラグにはしない。
add_library(katachi_warnings INTERFACE)
if(MSVC)
    target_compile_options(katachi_warnings INTERFACE /W4 /WX /permissive-)
else()
    target_compile_options(katachi_warnings INTERFACE -Wall -Wextra -Wpedantic -Werror)
endif()

# docs/phases.md §3: ASan + UBSan は macOS / Linux が対象。
add_library(katachi_sanitizers INTERFACE)
if(KATACHI_SANITIZE)
    if(MSVC)
        message(FATAL_ERROR "KATACHI_SANITIZE is not supported on MSVC (see docs/phases.md §3)")
    endif()
    target_compile_options(katachi_sanitizers INTERFACE -fsanitize=address,undefined
                                                        -fno-omit-frame-pointer -g)
    target_link_options(katachi_sanitizers INTERFACE -fsanitize=address,undefined)
endif()

enable_testing()

include(cmake/QualityGates.cmake)

add_subdirectory(src/core)
add_subdirectory(src/io)
add_subdirectory(src/app)
add_subdirectory(tests)

include(cmake/FormatMatrix.cmake)
