#
# Copyright (c) 2024 Ever Cars
#

target_include_directories(app PRIVATE .)
target_sources(app PRIVATE
	       ${CMAKE_CURRENT_SOURCE_DIR}/spi_event.c
	       ${CMAKE_CURRENT_SOURCE_DIR}/ui_terminal_event.c
)