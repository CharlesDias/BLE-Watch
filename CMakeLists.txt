# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# This command is necessary to use the display SSD1306
set(SHIELD ssd1306_128x64)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ble_watch)

set (APP_SOURCES 
   app/src/device_information_service.c
   app/src/display_ssd1306.c
   app/src/rtc_ds3231.c
)

target_sources(app PRIVATE 
   src/main.c
   ${APP_SOURCES}
)

target_include_directories(app PRIVATE
   app/inc
)