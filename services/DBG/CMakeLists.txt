# Copyright 2024-2025 NXP
# SPDX-License-Identifier: BSD-3-Clause

mcux_add_cmakelists(${CMAKE_CURRENT_LIST_DIR}/sbtsnoop)
mcux_add_cmakelists(${CMAKE_CURRENT_LIST_DIR}/SWO)

if(CONFIG_MCUX_COMPONENT_middleware.wireless.framework.dbg.fault_handlers)
    mcux_add_source(
        SOURCES
                fwk_fault_handlers.c
    )
endif()
