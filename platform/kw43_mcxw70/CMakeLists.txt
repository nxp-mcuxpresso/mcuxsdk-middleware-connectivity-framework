# Copyright 2025  NXP
# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_MCUX_PRJSEG_middleware.wireless.framework.platform.kw43_mcxw70)
    mcux_add_cmakelists(${CMAKE_CURRENT_LIST_DIR}/../wireless_mcu)

    if(CONFIG_MCUX_COMPONENT_middleware.wireless.framework.platform)
        mcux_add_source(SOURCES
            configs/fwk_config.h
            configs/rpmsg_config.h
            configs/SecLib_mbedtls_config.h
            fwk_platform_definitions.h
            DEVICE_IDS ${KW43_FAMILY}
        )

        mcux_add_include(INCLUDES
            .
            configs
            DEVICE_IDS ${KW43_FAMILY}
        )

        mcux_add_macro(
            REMOTE_CORE_BOOT_ADDRESS=0x02128000
        )
    endif()
endif()
