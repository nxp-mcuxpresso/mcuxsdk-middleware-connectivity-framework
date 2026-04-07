# Copyright 2025-2026 NXP
# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_MCUX_COMPONENT_middleware.wireless.framework.platform)
    mcux_add_source(SOURCES
        fwk_platform.c
        fwk_platform.h
        DEVICE_IDS ${KW47_FAMILY} ${MCXW72_FAMILY}
        CORE_IDS "cm33_core1"
    )
    mcux_add_source(SOURCES
        fwk_platform.c
        fwk_platform.h
        DEVICE_IDS "KW45B41Z83xxxA_NBU" "KW43B43ZC7xxx_NBU"
    )
    mcux_add_include(INCLUDES
        .
        ../include
        DEVICE_IDS ${KW47_FAMILY} ${MCXW72_FAMILY}
        CORE_IDS "cm33_core1"
    )
    mcux_add_include(INCLUDES
        .
        ../include
        DEVICE_IDS "KW45B41Z83xxxA_NBU" "KW43B43ZC7xxx_NBU"
    )
endif()

if(CONFIG_MCUX_COMPONENT_middleware.wireless.framework.platform.ble)
    mcux_add_source(SOURCES
        fwk_platform_ble.c
        fwk_platform_ble.h
        DEVICE_IDS ${KW47_FAMILY} ${MCXW72_FAMILY}
        CORE_IDS "cm33_core1"
    )
    mcux_add_source(SOURCES
        fwk_platform_ble.c
        fwk_platform_ble.h
        DEVICE_IDS "KW45B41Z83xxxA_NBU" "KW43B43ZC7xxx_NBU"
    )
endif()

if(CONFIG_MCUX_COMPONENT_middleware.wireless.framework.platform.ics)
    mcux_add_source(SOURCES
        fwk_platform_ics.c
        fwk_platform_ics.h
        DEVICE_IDS ${KW47_FAMILY} ${MCXW72_FAMILY}
        CORE_IDS "cm33_core1"
    )
    mcux_add_source(SOURCES
        fwk_platform_ics.c
        fwk_platform_ics.h
        DEVICE_IDS "KW45B41Z83xxxA_NBU" "KW43B43ZC7xxx_NBU"
    )
endif()

if(CONFIG_MCUX_COMPONENT_middleware.wireless.framework.platform.sensors)
    mcux_add_source(SOURCES
        fwk_platform_sensors.c
        fwk_platform_sensors.h
        DEVICE_IDS ${KW47_FAMILY} ${MCXW72_FAMILY}
        CORE_IDS "cm33_core1"
    )
    mcux_add_source(SOURCES
        fwk_platform_sensors.c
        fwk_platform_sensors.h
        DEVICE_IDS "KW45B41Z83xxxA_NBU" "KW43B43ZC7xxx_NBU"
    )
endif()

if(CONFIG_MCUX_COMPONENT_middleware.wireless.framework.platform.lowpower)
    mcux_add_source(SOURCES
        fwk_platform_lowpower.c
        fwk_platform_lowpower.h
        DEVICE_IDS ${KW47_FAMILY} ${MCXW72_FAMILY}
        CORE_IDS "cm33_core1"
    )
    mcux_add_source(SOURCES
        fwk_platform_lowpower.c
        fwk_platform_lowpower.h
        DEVICE_IDS "KW45B41Z83xxxA_NBU" "KW43B43ZC7xxx_NBU"
    )
endif()

if(CONFIG_MCUX_COMPONENT_middleware.wireless.framework.platform.genfsk)
    mcux_add_source(SOURCES
        fwk_platform_genfsk.c
        fwk_platform_genfsk.h
        DEVICE_IDS ${KW47_FAMILY} ${MCXW72_FAMILY}
        CORE_IDS "cm33_core1"
    )
    mcux_add_source(SOURCES
        fwk_platform_genfsk.c
        fwk_platform_genfsk.h
        DEVICE_IDS "KW45B41Z83xxxA_NBU" "KW43B43ZC7xxx_NBU"
    )
endif()

if(CONFIG_MCUX_COMPONENT_middleware.wireless.framework.platform.rng)
    mcux_add_source(SOURCES
        fwk_platform_rng.c
        ../include/fwk_platform_rng.h
        DEVICE_IDS ${KW47_FAMILY} ${MCXW72_FAMILY}
        CORE_IDS "cm33_core1"
    )
    mcux_add_source(SOURCES
        fwk_platform_rng.c
        ../include/fwk_platform_rng.h
        DEVICE_IDS "KW45B41Z83xxxA_NBU" "KW43B43ZC7xxx_NBU"
    )
endif()

if(CONFIG_MCUX_COMPONENT_middleware.wireless.framework.platform.dbg)
    mcux_add_source(SOURCES
        fwk_platform_dbg.c
        fwk_platform_dbg.h
        DEVICE_IDS ${KW47_FAMILY} ${MCXW72_FAMILY}
        CORE_IDS "cm33_core1"
    )
    mcux_add_source(SOURCES
        fwk_platform_dbg.c
        fwk_platform_dbg.h
        DEVICE_IDS "KW43B43ZC7xxx_NBU"
    )

    # fsl_assert has debug console dependency, map it to DEBUGCONSOLE_DISABLE (2U)
    mcux_add_macro(SDK_DEBUGCONSOLE=2U)
endif()
