cmake_minimum_required(VERSION 3.20)

project(try4 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(TRY4_WARNINGS_AS_ERRORS "Treat project warnings as errors" OFF)
option(TRY4_BUILD_TESTS "Build the test suite" ON)
option(TRY4_PORTABLE_WINDOWS "Use a self-contained Windows C++ runtime" ON)

if(MSVC AND TRY4_PORTABLE_WINDOWS)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

configure_file(cmake/version.hpp.in generated/chess/version.hpp @ONLY)

add_library(try4_core STATIC
    src/core/types.cpp
    src/core/position.cpp
    src/core/fen.cpp
    src/core/movegen.cpp
    src/core/notation.cpp
    src/core/perft.cpp
    src/core/zobrist.cpp
    src/core/evaluation.cpp
    src/core/search.cpp
    src/core/transposition_table.cpp
    src/core/time_manager.cpp
    src/core/opening_book.cpp
    src/core/configuration.cpp
    src/uci/uci.cpp
)
target_include_directories(try4_core PUBLIC
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
find_package(Threads REQUIRED)
target_link_libraries(try4_core PUBLIC Threads::Threads)

function(try4_enable_warnings target)
    if(MSVC)
        target_compile_options("${target}" PRIVATE /W4)
        if(TRY4_WARNINGS_AS_ERRORS)
            target_compile_options("${target}" PRIVATE /WX)
        endif()
    else()
        target_compile_options("${target}" PRIVATE -Wall -Wextra -Wpedantic)
        if(TRY4_WARNINGS_AS_ERRORS)
            target_compile_options("${target}" PRIVATE -Werror)
        endif()
    endif()
endfunction()

add_library(try4_cli STATIC src/cli/cli.cpp)
target_link_libraries(try4_cli PUBLIC try4_core)

add_executable(try4 apps/cli_main.cpp)
target_link_libraries(try4 PRIVATE try4_cli)

try4_enable_warnings(try4_core)
try4_enable_warnings(try4_cli)
try4_enable_warnings(try4)

add_executable(try4_perft_bench bench/perft_bench.cpp)
target_link_libraries(try4_perft_bench PRIVATE try4_core)
try4_enable_warnings(try4_perft_bench)

add_executable(try4_selfplay bench/selfplay.cpp)
target_link_libraries(try4_selfplay PRIVATE try4_core)
target_compile_definitions(try4_selfplay PRIVATE
    "TRY4_SELFPLAY_COMPILER=\"${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\""
    "TRY4_SELFPLAY_BUILD_TYPE=\"$<CONFIG>\""
)
try4_enable_warnings(try4_selfplay)

add_executable(try4_uci apps/uci_main.cpp)
target_link_libraries(try4_uci PRIVATE try4_core)
target_compile_definitions(try4_uci PRIVATE
    "TRY4_SELFPLAY_COMPILER=\"${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\""
    "TRY4_SELFPLAY_BUILD_TYPE=\"$<CONFIG>\""
)
try4_enable_warnings(try4_uci)

if(MINGW AND TRY4_PORTABLE_WINDOWS)
    target_link_options(try4 PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(try4_uci PRIVATE -static -static-libgcc -static-libstdc++)
endif()

include(GNUInstallDirs)
install(TARGETS try4 try4_uci RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES README.md CHANGELOG.md LICENSE CONTRIBUTING.md THIRD_PARTY_NOTICES.md
        DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(FILES docs/UCI.md docs/RELEASE.md docs/RELEASE_NOTES.md
        docs/CONFIGURATION.md docs/FORCING_SEARCH.md
        DESTINATION ${CMAKE_INSTALL_DOCDIR}/docs)
install(FILES config/try4.ini DESTINATION ${CMAKE_INSTALL_DOCDIR}/examples)

string(TOLOWER "${CMAKE_SYSTEM_NAME}" TRY4_PACKAGE_OS)
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" TRY4_PACKAGE_ARCH)
if(TRY4_PACKAGE_ARCH MATCHES "^(amd64|x64|x86_64)$")
    set(TRY4_PACKAGE_ARCH x86_64)
elseif(TRY4_PACKAGE_ARCH MATCHES "^(arm64|aarch64)$")
    set(TRY4_PACKAGE_ARCH aarch64)
endif()
set(CPACK_PACKAGE_NAME try4)
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "try4-${PROJECT_VERSION}-${TRY4_PACKAGE_OS}-${TRY4_PACKAGE_ARCH}")
set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/packages")
include(CPack)

if(TRY4_BUILD_TESTS)
    include(FetchContent)

    # Catch2 is fetched at configure time. Tests do not need network access at
    # runtime. If the build environment is offline, vendor Catch2 and replace
    # this declaration with add_subdirectory().
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.2
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Catch2)

    enable_testing()
    add_test(NAME try4_release_smoke COMMAND "${CMAKE_COMMAND}"
        "-DUCI=$<TARGET_FILE:try4_uci>" "-DCLI=$<TARGET_FILE:try4>"
        "-DEXPECTED_VERSION=${PROJECT_VERSION}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/release_smoke.cmake")
    set_tests_properties(try4_release_smoke PROPERTIES LABELS release TIMEOUT 15)

    function(try4_add_test target source label)
        add_executable("${target}" "${source}")
        target_link_libraries("${target}" PRIVATE try4_core Catch2::Catch2WithMain)
        try4_enable_warnings("${target}")
        add_test(NAME "${target}" COMMAND "${target}")
        set_tests_properties("${target}" PROPERTIES LABELS "${label}")
    endfunction()

    try4_add_test(try4_types tests/types_tests.cpp unit)
    try4_add_test(try4_position tests/position_tests.cpp unit)
    try4_add_test(try4_attack tests/attack_tests.cpp unit)
    try4_add_test(try4_movegen tests/movegen_tests.cpp unit)
    try4_add_test(try4_legal tests/legal_tests.cpp unit)
    try4_add_test(try4_result tests/result_tests.cpp unit)
    try4_add_test(try4_draw tests/draw_tests.cpp unit)
    try4_add_test(try4_notation tests/notation_tests.cpp unit)
    try4_add_test(try4_fen tests/fen_tests.cpp fen)
    try4_add_test(try4_fuzz tests/fuzz_tests.cpp fuzz)
    try4_add_test(try4_perft tests/perft_tests.cpp perft)
    try4_add_test(try4_property tests/property_tests.cpp property)
    try4_add_test(try4_evaluation tests/evaluation_tests.cpp unit)
    try4_add_test(try4_search tests/search_tests.cpp search)
    try4_add_test(try4_tactical tests/tactical_tests.cpp tactical)

    find_package(Threads REQUIRED)
    try4_add_test(try4_search_time tests/search_time_tests.cpp search)
    target_link_libraries(try4_search_time PRIVATE Threads::Threads)

    try4_add_test(try4_uci_tests tests/uci_tests.cpp uci)
    try4_add_test(try4_book_tests tests/opening_book_tests.cpp book)
    try4_add_test(try4_config_tests tests/configuration_tests.cpp config)
    try4_add_test(try4_forcing_tests tests/forcing_tests.cpp forcing)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME try4_engine_process COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/engine_process_tests.py"
        --engine "$<TARGET_FILE:try4_uci>" --source "${CMAKE_CURRENT_SOURCE_DIR}")
    set_tests_properties(try4_engine_process PROPERTIES LABELS "uci;config;forcing" TIMEOUT 90)
    target_compile_definitions(try4_config_tests PRIVATE
        "TRY4_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\"")
    target_compile_definitions(try4_book_tests PRIVATE
        "TRY4_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

    add_executable(try4_cli_tests tests/cli_tests.cpp)
    target_link_libraries(try4_cli_tests PRIVATE try4_cli Catch2::Catch2WithMain)
    try4_enable_warnings(try4_cli_tests)
    add_test(NAME try4_cli_tests COMMAND try4_cli_tests)
    set_tests_properties(try4_cli_tests PROPERTIES LABELS cli)

    target_compile_definitions(try4_fen PRIVATE
        "TRY4_TEST_DATA_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/tests/data/fen\"")
    target_compile_definitions(try4_perft PRIVATE
        "TRY4_PERFT_DATA_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/tests/data/perft\"")
    target_compile_definitions(try4_tactical PRIVATE
        "TRY4_TACTICAL_DATA_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/tests/data/tactical\"")
endif()
