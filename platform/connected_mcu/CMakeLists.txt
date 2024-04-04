set(SOURCES
    fwk_platform.c
    fwk_platform_ble.c
    fwk_platform_ot.c
    fwk_platform_ics.c
    fwk_platform_lowpower.c
    fwk_platform_lowpower_timer.c
    fwk_platform_reset.c
    fwk_platform_sensors.c
    fwk_platform_extflash.c
    fwk_platform_intflash.c
    fwk_platform_ota.c
    fwk_platform_mws.c
    fwk_platform_lcl.c
    fwk_platform_genfsk.c
)

if(CONNFWK_IGNORE_PLATFORM)
    list(REMOVE_ITEM SOURCES fwk_platform.c)
endif()

if(CONNFWK_IGNORE_PLATFORM_BLE)
    list(REMOVE_ITEM SOURCES fwk_platform_ble.c)
endif()

if(CONNFWK_IGNORE_PLATFORM_LCL)
    list(REMOVE_ITEM SOURCES fwk_platform_lcl.c)
endif()

if(CONNFWK_IGNORE_PLATFORM_GENFSK)
    list(REMOVE_ITEM SOURCES fwk_platform_genfsk.c)
endif()

if(CONNFWK_IGNORE_PLATFORM_LOWPOWER)
    list(REMOVE_ITEM SOURCES fwk_platform_lowpower.c)
    list(REMOVE_ITEM SOURCES fwk_platform_lowpower_timer.c)
endif()

if(CONNFWK_IGNORE_PLATFORM_SENSORS)
    list(REMOVE_ITEM SOURCES fwk_platform_sensors.c)
endif()

if(CONNFWK_IGNORE_PLATFORM_FLASH)
    list(REMOVE_ITEM SOURCES fwk_platform_flash.c)
endif()

if(CONNFWK_IGNORE_PLATFORM_ICS)
    list(REMOVE_ITEM SOURCES fwk_platform_ics.c)
endif()

if(CONNFWK_IGNORE_PLATFORM_OTA)
    list(REMOVE_ITEM SOURCES fwk_platform_ota.c)
endif()

if(CONNFWK_IGNORE_PLATFORM_MWS)
    list(REMOVE_ITEM SOURCES fwk_platform_mws.c)
endif()

add_library(${CONNFWK_PLATFORM_LIB} ${SOURCES})

# Making those includes PUBLIC will share them to the other targets linking this lib
target_include_directories(${CONNFWK_PLATFORM_LIB}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../NVS/Interface
        configs
)

# Get common configs from the connfwk-config interface
target_link_libraries(${CONNFWK_PLATFORM_LIB}
    PRIVATE
        connfwk-config
        connfwk-FunctionLib
        connfwk-SecLib
        connfwk-HWParameter
        connfwk-RNG
        connfwk-hdi
        connfwk-ipc
        connfwk-NVS
    PUBLIC
        ${CONNFWK_MCUX_SDK_LIB}
)

connfwk_target_set_link_symbol(${CONNFWK_PLATFORM_LIB} PUBLIC gLowpowerPowerDownEnable_d=1)

if(PROJECT_IS_TOP_LEVEL)
    connfwk_target_set_linker_script(${CONNFWK_PLATFORM_LIB} PUBLIC ${CONNFWK_PLATFORM_LINKER_FILE})
    # Rebuild executables if the linker script is changed
    set_target_properties(connfwk-config PROPERTIES INTERFACE_LINK_DEPENDS ${CONNFWK_PLATFORM_LINKER_FILE})
    connfwk_target_generate_map_file(${CONNFWK_PLATFORM_LIB} PUBLIC ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<TARGET_PROPERTY:NAME>.map)
endif()
