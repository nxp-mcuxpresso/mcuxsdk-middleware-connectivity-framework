add_library(${CONNFWK_BOARD_LIB}
    board.c
    app_services_init.c
    board_dcdc.c
    board_extflash.c
    hardware_init.c
    ${CONNFWK_PIN_MUX_VERSION}/clock_config.c
    ${CONNFWK_PIN_MUX_VERSION}/pin_mux.c
    ${CONNFWK_PIN_MUX_VERSION}/peripherals.c
)

target_include_directories(${CONNFWK_BOARD_LIB}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/${CONNFWK_PIN_MUX_VERSION}
        # ${CONNFWK_FREERTOS_CONFIG_PATH}
)

target_link_libraries(${CONNFWK_BOARD_LIB}
    PRIVATE
        connfwk-config
        ${CONNFWK_PLATFORM_LIB}
    PUBLIC
        ${CONNFWK_MCUX_SDK_LIB}
)
