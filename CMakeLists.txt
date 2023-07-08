# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# This command is necessary to use the display SSD1306
set(SHIELD ssd1306_128x64)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ble_watch)

# Global CMake variables are set here

# This is necessary for Sonarlint tools
# If enabled, generates a compile_commands.json file containing the exact compiler calls for all 
# translation units of the project in machine-readable form.
# Note: This option currently does not work well in combination with the UNITY_BUILD target 
# property or the CMAKE_UNITY_BUILD variable.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set (APP_SOURCES 
   src/app/src/device_information_service.c
   src/app/src/display_ssd1306.c
   src/app/src/gatt_central.c
   src/app/src/rtc_ds3231.c
)

target_sources(app PRIVATE 
   src/main.c
   ${APP_SOURCES}
)

target_include_directories(app PRIVATE
   src/app/inc
)