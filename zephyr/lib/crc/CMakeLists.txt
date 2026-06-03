# Copyright 2024, 2026 NXP
# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_CRC AND CONFIG_MCUX_COMPONENT_middleware.wireless.framework.zephyr_common)
    mcux_add_source(
        SOURCES ../../include/zephyr/sys/crc.h
                crc8_sw.c
    )
endif()
