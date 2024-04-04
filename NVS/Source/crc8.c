/*
 * Copyright (c) 2018 Workaround GmbH.
 * Copyright (c) 2017 Intel Corporation.
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 * Copyright (c) 2018 Google LLC.
 * Copyright (c) 2022 Meta
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "crc.h"

static const uint8_t crc8_ccitt_small_table[16] = {0x00u, 0x07u, 0x0eu, 0x09u, 0x1cu, 0x1bu, 0x12u, 0x15u,
                                                   0x38u, 0x3fu, 0x36u, 0x31u, 0x24u, 0x23u, 0x2au, 0x2du};

uint8_t crc8_ccitt(uint8_t val, const void *buf, size_t cnt)
{
    size_t         i;
    const uint8_t *p = buf;

    for (i = 0; i < cnt; i++)
    {
        val ^= p[i];
        val = (val << 4) ^ crc8_ccitt_small_table[val >> 4];
        val = (val << 4) ^ crc8_ccitt_small_table[val >> 4];
    }
    return val;
}