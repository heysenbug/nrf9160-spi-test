#
# Copyright (c) 2024 Evercars
#

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(spi_test)

# Include application application event headers
zephyr_library_include_directories(src/events)
zephyr_library_include_directories(src/modules)

# NORDIC SDK APP START
target_sources(app PRIVATE src/main.c)

target_sources(app PRIVATE
        src/events/spi_event.c
        src/events/ui_terminal_event.c
)

target_sources(app PRIVATE
        src/modules/spi_module.c
        src/modules/ui_terminal_module.c
        src/modules/button_module.c
)
# NORDIC SDK APP END
