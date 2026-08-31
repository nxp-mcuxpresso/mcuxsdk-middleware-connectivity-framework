# Copyright 2026 NXP
# SPDX-License-Identifier: BSD-3-Clause

mcux_set_list(RT685_FAMILY "MIMXRT685S")

if(CONFIG_MCUX_COMPONENT_middleware.wireless.framework.platform.MIMXRT685S)
    mcux_add_source(SOURCES
        fwk_platform.c
        fwk_platform.h
        DEVICE_IDS ${RT685_FAMILY}
    )

    mcux_add_include(INCLUDES
        .
    )
endif()

if(CONFIG_MCUX_COMPONENT_middleware.wireless.framework.platform.lowpower)
    mcux_add_source(SOURCES
        fwk_platform_lowpower.c
        fwk_platform_lowpower.h
        fwk_platform_lowpower_timer.h
        DEVICE_IDS ${RT685_FAMILY}
    )
endif()
