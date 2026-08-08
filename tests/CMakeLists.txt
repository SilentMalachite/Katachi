# docs/phases.md §1.5: Catch2 v3 は FetchContent + 固定タグ + FIND_PACKAGE_ARGS。
# タグはブランチ名やレンジで追跡せず、必ず固定する（再現性のため）。
include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.15.3
    GIT_SHALLOW TRUE
    FIND_PACKAGE_ARGS 3 NAMES Catch2)

FetchContent_MakeAvailable(Catch2)

add_executable(katachi_tests main.cpp core/qt_image_plugins_smoke.cpp)

target_link_libraries(
    katachi_tests
    PRIVATE Catch2::Catch2
            Qt6::Gui
            katachi_warnings
            katachi_sanitizers)

list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)
catch_discover_tests(katachi_tests)
