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

# docs/phases.md §2.4: フィクスチャは自前生成のみ。生成器を残す。
# 生成物はコミットせずビルド時に作る。生成器と成果物が乖離しないため。
set(KATACHI_FIXTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/fixtures")

add_executable(katachi_fixture_gen fixtures/generate.cpp)

target_link_libraries(katachi_fixture_gen PRIVATE Qt6::Gui katachi_warnings)

# 生成器は各フィクスチャの大きさ（< 50KB）と、意図した性質が
# 実際に保持されているかを自分で検証し、駄目なら非ゼロ終了する。
add_custom_command(
    OUTPUT "${KATACHI_FIXTURE_DIR}/gradient_rgb.png"
    COMMAND katachi_fixture_gen "${KATACHI_FIXTURE_DIR}"
    DEPENDS katachi_fixture_gen
    COMMENT "テストフィクスチャを生成"
    VERBATIM)

add_custom_target(katachi_fixtures ALL DEPENDS "${KATACHI_FIXTURE_DIR}/gradient_rgb.png")

add_executable(katachi_tests main.cpp core/qt_image_plugins_smoke.cpp core/result_test.cpp
                            core/format_id_test.cpp
                            core/capability_table_test.cpp
                            core/fixtures_test.cpp
                            core/converter_test.cpp)

add_dependencies(katachi_tests katachi_fixtures)

target_compile_definitions(katachi_tests PRIVATE KATACHI_FIXTURE_DIR="${KATACHI_FIXTURE_DIR}")

target_link_libraries(
    katachi_tests
    PRIVATE Catch2::Catch2
            Qt6::Gui
            katachi_core
            katachi_warnings
            katachi_sanitizers)

list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)
catch_discover_tests(katachi_tests)

# Result<T,E> の T == E 排除はコンパイルエラーとして現れるため、実行時テストでは観測できない。
# 「ビルドが失敗すること」を確認する（不変条件スキャナのネガティブテストと同じ考え方）。
# EXCLUDE_FROM_ALL のため通常のビルドでは作られない。
add_executable(katachi_result_same_type_guard EXCLUDE_FROM_ALL
               core/compile_fail/result_same_type.cpp)

target_link_libraries(katachi_result_same_type_guard PRIVATE katachi_core)

add_test(NAME core.result.rejects_same_type
         COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target
                 katachi_result_same_type_guard)

set_tests_properties(core.result.rejects_same_type PROPERTIES WILL_FAIL TRUE)
