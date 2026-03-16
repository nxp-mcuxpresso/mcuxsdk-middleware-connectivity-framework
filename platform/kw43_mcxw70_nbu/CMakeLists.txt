# Copyright 2025-2026 NXP
# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_MCUX_COMPONENT_middleware.wireless.framework.platform.kw43_mcxw70_nbu)
    mcux_add_cmakelists(${CMAKE_CURRENT_LIST_DIR}/../wireless_nbu)
    mcux_add_source(SOURCES
        configs/fwk_config.h
        configs/rpmsg_config.h
        fwk_platform_kw43_mcxw70_nbu.c
        DEVICE_IDS "KW43B43ZC7xxxA_NBU"
    )
    mcux_add_include(INCLUDES
        .
        configs
        DEVICE_IDS "KW43B43ZC7xxxA_NBU"
    )
endif()
