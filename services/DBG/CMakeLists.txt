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

if(CONFIG_MCUX_COMPONENT_middleware.wireless.framework.dbg.nbu_debug_on_host)
    mcux_add_source(
        SOURCES
                nbu_dbg/host_interface/fwk_nbu_dbg.c
                nbu_dbg/host_interface/fwk_nbu_dbg.h
    )

    mcux_add_include(
        INCLUDES
                nbu_dbg/host_interface
    )
endif()

if(CONFIG_MCUX_COMPONENT_middleware.wireless.framework.dbg.nbu_debug)
    mcux_add_source(
        SOURCES
                nbu_dbg/nbu_interface/fwk_nbu_dbg.c
    )

    mcux_add_include(
        INCLUDES
                nbu_dbg/nbu_interface
    )
endif()

if(CONFIG_MCUX_COMPONENT_middleware.wireless.framework.dbg.nbu_debug.dbg_struct_legacy)
    mcux_add_source(
        SOURCES
                nbu_dbg/common/legacy/fwk_debug_struct.h
    )

    mcux_add_include(
        INCLUDES
                nbu_dbg/common/legacy
    )
endif()

if(CONFIG_MCUX_COMPONENT_middleware.wireless.framework.dbg.nbu_debug.dbg_struct)
    mcux_add_source(
        SOURCES
                nbu_dbg/common/fwk_debug_struct.h
    )

    mcux_add_include(
        INCLUDES
                nbu_dbg/common
    )
endif()
