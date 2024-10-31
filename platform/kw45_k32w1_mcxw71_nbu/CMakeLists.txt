# Copyright 2024 NXP
# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_MCUX_PRJSEG_middleware.wireless.framework.platform.kw45_mcxw71_nbu)
    mcux_add_cmakelists(${CMAKE_CURRENT_LIST_DIR}/../wireless_nbu)

    if(CONFIG_MCUX_COMPONENT_middleware.wireless.framework.platform)
        mcux_add_source(SOURCES
            configs/fwk_config.h
            configs/rpmsg_config.h
            CORE_IDS "cm33_core1"
        )
        mcux_add_source(SOURCES
            configs/fwk_config.h
            configs/rpmsg_config.h
            DEVICE_IDS "KW45B41Z83xxxA_NBU"
        )
        mcux_add_include(INCLUDES
            .
            configs
            CORE_IDS "cm33_core1"
        )
        mcux_add_include(INCLUDES
            .
            configs
            DEVICE_IDS "KW45B41Z83xxxA_NBU"
        )
    endif()
endif()
