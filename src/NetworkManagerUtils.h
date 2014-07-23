/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager -- Network link manager
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright (C) 2004 - 2014 Red Hat, Inc.
 * Copyright (C) 2005 - 2008 Novell, Inc.
 */

#ifndef NETWORK_MANAGER_UTILS_H
#define NETWORK_MANAGER_UTILS_H

#include <glib.h>
#include <stdio.h>
#include <net/ethernet.h>

#include "nm-connection.h"

gboolean nm_ethernet_address_is_valid (const struct ether_addr *test_addr);

in_addr_t nm_utils_ip4_address_clear_host_address (in_addr_t addr, guint8 plen);
void nm_utils_ip6_address_clear_host_address (struct in6_addr *dst, const struct in6_addr *src, guint8 plen);

int nm_spawn_process (const char *args);

/* macro to return strlen() of a compile time string. */
#define STRLEN(str)     ( sizeof ("" str) - 1 )

/**
 * str_if_set:
 * @str: input string that will be returned if @str is not %NULL
 * @fallback: if @str is %NULL, return @fallback instead
 *
 * This utility function is useful when printing a string to avoid passing
 * %NULL. E.g. printf ("%s", str_if_set (get_string(), "(none)"));
 *
 * Returns: either @str or @fallback, depending on whether @str is %NULL.
 */
static inline const char *
str_if_set (const char *str, const char *fallback)
{
	return str ? str : fallback;
}

typedef void (*NMUtilsKillChildAsyncCb) (pid_t pid, gboolean success, int child_status, void *user_data);
void nm_utils_kill_child_async (pid_t pid, int sig, guint64 log_domain, const char *log_name,
                                guint32 wait_before_kill_msec,
                                NMUtilsKillChildAsyncCb callback, void *user_data);
gboolean nm_utils_kill_child_sync (pid_t pid, int sig, guint64 log_domain, const char *log_name,
                                   int *child_status, guint32 wait_before_kill_msec,
                                   guint32 sleep_duration_msec);

gboolean nm_match_spec_string (const GSList *specs, const char *string);
gboolean nm_match_spec_hwaddr (const GSList *specs, const char *hwaddr);
gboolean nm_match_spec_s390_subchannels (const GSList *specs, const char *subchannels);
gboolean nm_match_spec_interface_name (const GSList *specs, const char *interface_name);

const char *nm_utils_get_shared_wifi_permission (NMConnection *connection);

GHashTable *value_hash_create          (void);
void        value_hash_add             (GHashTable *hash,
										const char *key,
										GValue *value);

void        value_hash_add_str         (GHashTable *hash,
										const char *key,
										const char *str);

void        value_hash_add_object_path (GHashTable *hash,
										const char *key,
										const char *op);

void        value_hash_add_uint        (GHashTable *hash,
										const char *key,
										guint32 val);

void        value_hash_add_bool        (GHashTable *hash,
					                    const char *key,
					                    gboolean val);

void        value_hash_add_object_property (GHashTable *hash,
                                            const char *key,
                                            GObject *object,
                                            const char *prop,
                                            GType val_type);

const char *nm_utils_get_ip_config_method (NMConnection *connection,
                                           GType         ip_setting_type);

void nm_utils_complete_generic (NMConnection *connection,
                                const char *ctype,
                                const GSList *existing,
                                const char *format,
                                const char *preferred,
                                gboolean default_enable_ipv6);

char *nm_utils_new_vlan_name (const char *parent_iface, guint32 vlan_id);

GPtrArray *nm_utils_read_resolv_conf_nameservers (const char *rc_contents);

typedef gboolean (NMUtilsMatchFilterFunc) (NMConnection *connection, gpointer user_data);

NMConnection *nm_utils_match_connection (GSList *connections,
                                         NMConnection *original,
                                         gboolean device_has_carrier,
                                         NMUtilsMatchFilterFunc match_filter_func,
                                         gpointer match_filter_data);

gint64 nm_utils_ascii_str_to_int64 (const char *str, guint base, gint64 min, gint64 max, gint64 fallback);

#define NM_UTILS_NS_PER_SECOND  ((gint64) 1000000000)
gint64 nm_utils_get_monotonic_timestamp_ns (void);
gint64 nm_utils_get_monotonic_timestamp_us (void);
gint64 nm_utils_get_monotonic_timestamp_ms (void);
gint32 nm_utils_get_monotonic_timestamp_s (void);

const char *ASSERT_VALID_PATH_COMPONENT (const char *name) G_GNUC_WARN_UNUSED_RESULT;
const char *nm_utils_ip6_property_path (const char *ifname, const char *property);

gboolean nm_utils_is_specific_hostname (const char *name);

#endif /* NETWORK_MANAGER_UTILS_H */
