/*
 * Copyright 2024 NXP
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * \file
 *
 * This source code is imported from Zephyr project.
 * Note a few include modifications were required to make it compile in a non
 * Zephyr environment.
 *
 */
#include <string.h>

#ifdef ZEPHYR_SETTINGS_ORIGINAL_SOURCE
#include <errno.h>
#include <zephyr/sys/util.h>

#include "settings_priv.h"
#else
#include "nvs_port.h"
#endif
#include <zephyr/settings/settings.h>
struct read_cb_arg {
	const void *data;
	size_t len;
};

static ssize_t settings_runtime_read_cb(void *cb_arg, void *data, size_t len)
{
	struct read_cb_arg *arg = (struct read_cb_arg *)cb_arg;

	memcpy(data, arg->data, MIN(arg->len, len));
	return MIN(arg->len, len);
}

int settings_runtime_set(const char *name, const void *data, size_t len)
{
	struct settings_handler_static *ch;
	const char *name_key;
	struct read_cb_arg arg;

	ch = settings_parse_and_lookup(name, &name_key);
	if (!ch) {
		return -EINVAL;
	}

	arg.data = data;
	arg.len = len;
	return ch->h_set(name_key, len, settings_runtime_read_cb, (void *)&arg);
}

int settings_runtime_get(const char *name, void *data, size_t len)
{
	struct settings_handler_static *ch;
	const char *name_key;

	ch = settings_parse_and_lookup(name, &name_key);
	if (!ch) {
		return -EINVAL;
	}

	if (!ch->h_get) {
		return -ENOTSUP;
	}

	return ch->h_get(name_key, data, len);
}

int settings_runtime_commit(const char *name)
{
	struct settings_handler_static *ch;
	const char *name_key;

	ch = settings_parse_and_lookup(name, &name_key);
	if (!ch) {
		return -EINVAL;
	}

	if (ch->h_commit) {
		return ch->h_commit();
	} else {
		return 0;
	}
}
