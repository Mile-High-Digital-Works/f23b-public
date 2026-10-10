# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.25)
project(F23B_Grinnelli_Flight_Feel VERSION 0.2.0 LANGUAGES CXX)
include(CTest)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(DCS_ROOT "$ENV{DCS_ROOT}" CACHE PATH "Root of local DCS World installation")

add_library(f23b_performance_core STATIC
    src/efm/performance/performance_model.cpp
    src/efm/performance/engine_mapping.cpp)
target_include_directories(f23b_performance_core PUBLIC
    src/efm/performance/include)
target_compile_features(f23b_performance_core PUBLIC cxx_std_20)
target_compile_options(f23b_performance_core PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>)
set_target_properties(f23b_performance_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_executable(f23b_performance_model_tests
    tests/native/f23b_performance_model_tests.cpp)
target_link_libraries(f23b_performance_model_tests PRIVATE f23b_performance_core)
target_compile_features(f23b_performance_model_tests PRIVATE cxx_std_20)
add_test(NAME f23b_performance_model_tests COMMAND f23b_performance_model_tests)

add_executable(f23b_roll_stability_tests tests/native/f23b_roll_stability_tests.cpp)
target_link_libraries(f23b_roll_stability_tests PRIVATE f23b_performance_core)
add_test(NAME f23b_roll_stability_tests COMMAND f23b_roll_stability_tests)

add_executable(f23b_performance_engine_mapping_tests
    tests/native/f23b_performance_engine_mapping_tests.cpp)
target_link_libraries(f23b_performance_engine_mapping_tests PRIVATE f23b_performance_core)
target_compile_features(f23b_performance_engine_mapping_tests PRIVATE cxx_std_20)
add_test(NAME f23b_performance_engine_mapping_tests
    COMMAND f23b_performance_engine_mapping_tests)

add_executable(f23b_performance_report
    tools/performance/performance_report.cpp)
target_link_libraries(f23b_performance_report PRIVATE f23b_performance_core)
target_compile_features(f23b_performance_report PRIVATE cxx_std_20)

# The rejected Hornet/custom-EFM activation is retired. Compile callbacks into
# tests only; real SDK ABI types are checked locally, stub used by portable CI.
add_executable(f23b_performance_adapter_tests tests/native/f23b_performance_adapter_tests.cpp)
target_link_libraries(f23b_performance_adapter_tests PRIVATE f23b_performance_core)
if(EXISTS "${DCS_ROOT}/API/include/FM/wHumanCustomPhysicsAPI.h")
    target_include_directories(f23b_performance_adapter_tests PRIVATE "${DCS_ROOT}/API/include")
    target_compile_definitions(f23b_performance_adapter_tests PRIVATE F23B_REAL_SDK=1)
else()
    target_include_directories(f23b_performance_adapter_tests PRIVATE tests/stubs)
endif()
add_test(NAME f23b_performance_adapter_tests COMMAND f23b_performance_adapter_tests)
