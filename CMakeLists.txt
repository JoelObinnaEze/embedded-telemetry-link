cmake_minimum_required(VERSION 3.20)
project(embedded_telemetry_link VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

add_library(telemetry_link src/telemetry_link.c)
target_include_directories(telemetry_link PUBLIC include)

add_executable(telemetry_demo app/demo.c)
target_link_libraries(telemetry_demo PRIVATE telemetry_link)

enable_testing()
add_executable(telemetry_link_tests tests/test_telemetry_link.c)
target_link_libraries(telemetry_link_tests PRIVATE telemetry_link)
add_test(NAME telemetry_link_tests COMMAND telemetry_link_tests)
