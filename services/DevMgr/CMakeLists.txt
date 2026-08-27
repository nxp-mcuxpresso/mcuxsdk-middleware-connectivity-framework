# Copyright 2026 NXP
# SPDX-License-Identifier: BSD-3-Clause

if (CONFIG_MCUX_COMPONENT_middleware.wireless.framework.devmgr)

    mcux_add_source(
        SOURCES fwk_devmgr.h
                fwk_devmgr_common.c
                fwk_devmgr_common.h
                README.md
    )

    if (CONFIG_MCUX_COMPONENT_middleware.wireless.framework.devmgr_hwparam)
        mcux_add_source(SOURCES fwk_devmgr_hwparam.c)
    endif()

    if (CONFIG_MCUX_COMPONENT_middleware.wireless.framework.devmgr_plat)
        mcux_add_source(SOURCES fwk_devmgr_platf.c)
    endif()

    if (CONFIG_MCUX_COMPONENT_middleware.wireless.framework.devmgr_mac_gen)
        mcux_add_source(
            SOURCES fwk_devmgr_mac_gen.c
                    fwk_devmgr_mac_gen.h
        )
        # Signals the HWParameter backend to build in the generation fallback.
        mcux_add_macro(DEVMGR_MAC_GEN_ENABLED=1)
    endif()

    mcux_add_include(
        INCLUDES .
    )
endif()
