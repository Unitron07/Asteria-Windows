cmake_minimum_required(VERSION 3.27)
project(asteria_input_regressions LANGUAGES CXX)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/generate_keyboard_test.py"
    "${CMAKE_CURRENT_BINARY_DIR}/keyboard_test.cpp" COMMAND_ERROR_IS_FATAL ANY)
add_executable(keyboard-tests "${CMAKE_CURRENT_BINARY_DIR}/keyboard_test.cpp")
target_compile_features(keyboard-tests PRIVATE cxx_std_17)
target_include_directories(keyboard-tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../libs/windows/include/${SDL_ARCH}/SDL2"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../moonlight-common-c/moonlight-common-c/src")
enable_testing()
add_test(NAME keyboard-mapping-release-state COMMAND keyboard-tests)
