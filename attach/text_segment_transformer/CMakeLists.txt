# The rewriting itself, without the __libc_start_main interposer of the
# preload library, so that tests can apply it to their own process.
add_library(bpftime_text_segment_transformer_core STATIC
    text_segment_transformer.cpp
    agent_namespace.cpp
)
add_dependencies(bpftime_text_segment_transformer_core spdlog::spdlog FridaGum)
set_target_properties(bpftime_text_segment_transformer_core PROPERTIES CXX_STANDARD 20 POSITION_INDEPENDENT_CODE ON)
target_link_libraries(bpftime_text_segment_transformer_core
    PUBLIC
    spdlog::spdlog
    ${FRIDA_GUM_INSTALL_DIR}/libfrida-gum.a
)
target_include_directories(bpftime_text_segment_transformer_core
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${FRIDA_GUM_INSTALL_DIR}
    ${SPDLOG_INCLUDE}
)

add_library(bpftime_text_segment_transformer SHARED
    agent-transformer.cpp
)
add_dependencies(bpftime_text_segment_transformer spdlog::spdlog FridaGum)
set_target_properties(bpftime_text_segment_transformer PROPERTIES CXX_STANDARD 20 OUTPUT_NAME "bpftime-agent-transformer")

target_link_libraries(bpftime_text_segment_transformer
    -Wl,--whole-archive bpftime_text_segment_transformer_core -Wl,--no-whole-archive
    spdlog::spdlog
    ${FRIDA_GUM_INSTALL_DIR}/libfrida-gum.a
)

target_include_directories(bpftime_text_segment_transformer
    PRIVATE
    ${FRIDA_GUM_INSTALL_DIR}
    ${SPDLOG_INCLUDE}
)

if (${TEST_LCOV})
    target_compile_options(bpftime_text_segment_transformer PRIVATE -fprofile-arcs -ftest-coverage -fprofile-update=atomic)
    target_link_options(bpftime_text_segment_transformer PRIVATE -fprofile-arcs)
endif()

# The agent namespace loader on every architecture; the rewriting cases on
# aarch64, where each case rewrites a forked child of the test process.
set(TRANSFORMER_TEST_SOURCES test/test_agent_namespace.cpp)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    enable_language(ASM)
    list(APPEND TRANSFORMER_TEST_SOURCES
        test/test_syscall_rewrite.cpp
        test/regs_check_aarch64.S
    )
endif()
add_library(bpftime_test_fake_agent_keys MODULE test/fake_agent_keys.c)
add_executable(bpftime_text_segment_transformer_tests ${TRANSFORMER_TEST_SOURCES})
if(${ENABLE_EBPF_VERIFIER} AND NOT TARGET Catch2)
    Include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.4.0
    )
    FetchContent_MakeAvailable(Catch2)
endif()
add_dependencies(bpftime_text_segment_transformer_tests Catch2 bpftime_text_segment_transformer_core bpftime_test_fake_agent_keys)
target_compile_definitions(bpftime_text_segment_transformer_tests PRIVATE
    FAKE_AGENT_KEYS_PATH="$<TARGET_FILE:bpftime_test_fake_agent_keys>")
target_link_libraries(bpftime_text_segment_transformer_tests PRIVATE Catch2::Catch2WithMain bpftime_text_segment_transformer_core pthread)
target_include_directories(bpftime_text_segment_transformer_tests PRIVATE ${Catch2_INCLUDE})
set_property(TARGET bpftime_text_segment_transformer_tests PROPERTY CXX_STANDARD 20)
add_test(NAME bpftime_text_segment_transformer_tests COMMAND bpftime_text_segment_transformer_tests)
