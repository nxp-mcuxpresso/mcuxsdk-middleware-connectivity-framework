# Copyright 2024 NXP
# All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause

add_library(${CONNFWK_BOARD_LIB}
    board.c
    pin_mux.c
    clock_config.c
    hardware_init.c
    ../rdrw612bga/app_services_init.c
)

if(CONNFWK_BOARD_INCLUDE_COMP)
    target_sources(${CONNFWK_BOARD_LIB} PRIVATE
        board_comp.c
    )
endif()

if(CONNFWK_FSABSTRACTION MATCHES "LFS")
    target_sources(${CONNFWK_BOARD_LIB} PRIVATE
        ../rdrw612bga/peripherals/peripherals.c
    )
endif()

target_include_directories(${CONNFWK_BOARD_LIB}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ../rdrw612bga/peripherals
        ${CONNFWK_FREERTOS_CONFIG_PATH}
)

target_link_libraries(${CONNFWK_BOARD_LIB}
    PRIVATE
        connfwk-config
        ${CONNFWK_PLATFORM_LIB}
    PUBLIC
        ${CONNFWK_MCUX_SDK_LIB}
)

target_link_libraries(connfwk-config
    INTERFACE
        ${CONNFWK_BOARD_LIB}
)

target_compile_definitions(${CONNFWK_BOARD_LIB}
    PUBLIC
        -DBOARD_DEBUG_UART_BAUDRATE=115200
        -DBOOT_HEADER_ENABLE=1
)
