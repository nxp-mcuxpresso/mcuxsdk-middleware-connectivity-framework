# Copyright 2024 NXP
# SPDX-License-Identifier: Apache-2.0

mcux_add_source(
    SOURCES ../../../include/zephyr/settings/settings.h
            settings.c
            settings_init.c
            settings_store.c
)

# Pa134: suppress "pointless integer comparison" triggered by SETTINGS_NAME_END == '\0'
# comparisons in upstream Zephyr code (settings_port.h redefines SETTINGS_NAME_END to '\0').
# Code from upstream, avoiding local patches
mcux_add_iar_configuration(
    CC "--diag_suppress=Pa134"
    CX "--diag_suppress=Pa134"
)

if(CONFIG_SETTINGS_NVS)
    mcux_add_source(
        SOURCES ../include/settings/settings_nvs.h
                settings_nvs.c
    )
endif()

if(CONFIG_SETTINGS_RUNTIME)
    mcux_add_source(
        SOURCES settings_runtime.c
    )
endif()
