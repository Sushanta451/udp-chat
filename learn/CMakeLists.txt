# Builds the C++ crash course: one program per chapter, plus the assignment.
# This is separate from the main udpchat build on purpose.
#
#   cmake -S learn -B learn/build
#   cmake --build learn/build
#   ./learn/build/ch10_type_conversion
#   ctest --test-dir learn/build --output-on-failure
cmake_minimum_required(VERSION 3.20)
project(udpchat_learn CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Reuse the main project's warning flags.
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../cmake)
include(CompilerWarnings)

# Every chapters/chNN_name.cpp becomes a program called chNN_name,
# every networking/netN_name.cpp becomes netN_name, and every beej/*.cpp too.
file(GLOB LESSONS CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/chapters/*.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/networking/*.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/beej/*.cpp)
foreach(lesson ${LESSONS})
  get_filename_component(name ${lesson} NAME_WE)
  add_executable(${name} ${lesson})
  set_project_warnings(${name})
endforeach()

# The assignment: a library with your code, and a test program that uses it.
# This is the same layout the real chat should end up with (see networking/net2_program_structure.cpp).
add_library(chatcore assignment/packet.cpp assignment/sequence_tracker.cpp)
target_include_directories(chatcore PUBLIC assignment)
set_project_warnings(chatcore)

add_executable(assignment_tests assignment/tests.cpp)
target_link_libraries(assignment_tests PRIVATE chatcore)
set_project_warnings(assignment_tests)

enable_testing()
add_test(NAME assignment COMMAND assignment_tests)
