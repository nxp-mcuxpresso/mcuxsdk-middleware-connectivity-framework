# Copyright 2025 NXP
# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_MCUX_PRJSEG_middleware.wireless.framework.platform.kw43_mcxw70_nbu)
    mcux_add_cmakelists(${CMAKE_CURRENT_LIST_DIR}/../wireless_nbu)

    if(CONFIG_MCUX_COMPONENT_middleware.wireless.framework.platform)
        mcux_add_source(SOURCES
            configs/fwk_config.h
            configs/rpmsg_config.h
            DEVICE_IDS "KW43B43ZC7xxxA_NBU"
        )
        mcux_add_include(INCLUDES
            .
            configs
            DEVICE_IDS "KW43B43ZC7xxxA_NBU"
        )
    endif()
endif()
