# Copyright 2026 NXP
# SPDX-License-Identifier: BSD-3-Clause

if (CONFIG_MCUX_COMPONENT_middleware.wireless.framework.devmgr)

    mcux_add_source(
        SOURCES fwk_devmgr.h
                fwk_devmgr_common.c
                fwk_devmgr_common.h
                README.md
    )

    # Signals manufacturing-data consumers (framework platform code, bluetooth
    # and ieee-802.15.4) to route BD address / IEEE 802.15.4 address / crystal
    # trim access through the DEVMGR_* API instead of the legacy direct
    # Hardware Parameter path. Emitted whenever the DevMgr component is built so
    # that every consumer whose Kconfig selects DevMgr compiles its DEVMGR_*
    # code path in.

    if (CONFIG_MCUX_COMPONENT_middleware.wireless.framework.devmgr_hwparam)
        mcux_add_source(SOURCES fwk_devmgr_hwparam.c)
    endif()

    # The platform/CFPA backend supplies the same public DEVMGR_* API as the
    # HWParameter backend. Exactly one backend .c must be compiled (see the
    # devmgr_backend Kconfig choice); compile the platform backend whenever it
    # is the selected backend, otherwise the DEVMGR_* symbols are undefined at
    # link time on devices that default to it (e.g. KW43/MCXW70, MCXW23).
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
