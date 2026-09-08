# Copyright 2024, 2026 NXP
# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_MCUX_COMPONENT_middleware.wireless.framework.zephyr_lib.crc)
    mcux_add_source(
        SOURCES ../../include/zephyr/sys/crc.h
                crc8_sw.c
                crc16_sw.c
    )
endif()
