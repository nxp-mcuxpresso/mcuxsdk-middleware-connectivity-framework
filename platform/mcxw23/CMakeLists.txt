# Copyright 2025 NXP
# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_MCUX_PRJSEG_middleware.wireless.framework.platform.mcxw23)

    mcux_set_list(MCXW23_FAMILY "MCXW236B MCXW235B")

    if(CONFIG_MCUX_COMPONENT_middleware.wireless.framework.platform)
      mcux_add_source(
        SOURCES
        configs/fwk_config.h
        fwk_platform_definitions.h
        fwk_platform.c
        ../include/fwk_platform_rng.h
        ../wireless_mcu/fwk_platform.h
        ../wireless_mcu/fwk_platform_ics.h
        DEVICE_IDS ${MCXW23_FAMILY}
        CORE_IDS "cm33"
      )

      mcux_add_include(INCLUDES 
        .
        ../include
        ../wireless_mcu
        configs
        DEVICE_IDS ${MCXW23_FAMILY}
      )
      
      mcux_add_configuration(CC "-DENABLE_RAM_VECTOR_TABLE")
    endif()

    if(CONFIG_MCUX_COMPONENT_middleware.wireless.framework.platform.ble)
      mcux_add_source(
        SOURCES
        ../wireless_mcu/fwk_platform_ble.h
        DEVICE_IDS ${MCXW23_FAMILY}
        CORE_IDS "cm33"
      )

      mcux_add_include(INCLUDES 
        ../wireless_mcu
        DEVICE_IDS ${MCXW23_FAMILY}
      )
    endif()

    if(CONFIG_MCUX_COMPONENT_middleware.wireless.framework.platform.reset)
        mcux_add_source(
        SOURCES
        ../wireless_mcu/fwk_platform_reset.h
        DEVICE_IDS ${MCXW23_FAMILY}
        CORE_IDS "cm33"
      )
    endif()

endif()
