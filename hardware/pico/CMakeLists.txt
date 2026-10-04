cmake_minimum_required(VERSION 3.20)

if(NOT PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
  set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
endif()
if(NOT PICO_SDK_PATH)
  message(FATAL_ERROR "Set PICO_SDK_PATH to a Raspberry Pi Pico SDK checkout")
endif()

include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")

project(telemetry_pico_demo C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

pico_sdk_init()

if(PICO_SDK_VERSION_STRING VERSION_LESS "2.0.0")
  message(FATAL_ERROR "Pico SDK 2.0.0 or newer is required for Pico 2 support")
endif()

set(ETL_PICO_SOURCES
  main.c
  ../../src/telemetry_link.c
)
add_executable(telemetry_pico ${ETL_PICO_SOURCES})
target_include_directories(telemetry_pico PRIVATE ../../include)
set_source_files_properties(${ETL_PICO_SOURCES} PROPERTIES
  COMPILE_OPTIONS "-Wall;-Wextra;-Wpedantic;-Werror"
)
target_link_libraries(telemetry_pico PRIVATE pico_stdlib hardware_uart)

pico_enable_stdio_usb(telemetry_pico 1)
pico_enable_stdio_uart(telemetry_pico 0)
pico_add_extra_outputs(telemetry_pico)
