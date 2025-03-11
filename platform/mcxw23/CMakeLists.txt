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
        ../wireless_mcu/fwk_platform.h
        ../include/fwk_platform_rng.h
        ../wireless_mcu/fwk_platform_ble.h
        DEVICE_IDS ${MCXW23_FAMILY}
        CORE_IDS "cm33_core0" "cm33"
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
endif()
