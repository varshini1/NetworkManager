/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * libnm_glib -- Access network status & information from glib applications
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 * Copyright (C) 2007 - 2008 Novell, Inc.
 * Copyright (C) 2007 - 2011 Red Hat, Inc.
 */

#include <string.h>
#include <gio/gio.h>
#include <nm-utils.h>
#include "NetworkManager.h"
#include "nm-object.h"
#include "nm-object-cache.h"
#include "nm-object-private.h"
#include "nm-dbus-glib-types.h"
#include "nm-glib-compat.h"

#define DEBUG 0

G_DEFINE_ABSTRACT_TYPE (NMObject, nm_object, G_TYPE_OBJECT)

#define NM_OBJECT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NM_TYPE_OBJECT, NMObjectPrivate))

typedef struct {
	PropertyMarshalFunc func;
	gpointer field;
} PropertyInfo;

typedef struct {
	DBusGConnection *connection;
	char *path;
	DBusGProxy *properties_proxy;
	GSList *property_interfaces;
	GSList *property_tables;
	NMObject *parent;

	GSList *notify_props;
	guint32 notify_id;
	gboolean inited, disposed;
} NMObjectPrivate;

enum {
	PROP_0,
	PROP_CONNECTION,
	PROP_PATH,

	LAST_PROP
};

static void
nm_object_init (NMObject *object)
{
}

static GObject*
constructor (GType type,
			 guint n_construct_params,
			 GObjectConstructParam *construct_params)
{
	GObject *object;
	NMObjectPrivate *priv;

	object = G_OBJECT_CLASS (nm_object_parent_class)->constructor (type,
																   n_construct_params,
																   construct_params);
	if (!object)
		return NULL;

	priv = NM_OBJECT_GET_PRIVATE (object);

	if (priv->connection == NULL || priv->path == NULL) {
		g_warning ("%s: bus connection and path required.", __func__);
		g_object_unref (object);
		return NULL;
	}

	priv->properties_proxy = dbus_g_proxy_new_for_name (priv->connection,
														NM_DBUS_SERVICE,
														priv->path,
														"org.freedesktop.DBus.Properties");

	_nm_object_cache_add (NM_OBJECT (object));

	return object;
}

static void
dispose (GObject *object)
{
	NMObjectPrivate *priv = NM_OBJECT_GET_PRIVATE (object);

	if (priv->disposed) {
		G_OBJECT_CLASS (nm_object_parent_class)->dispose (object);
		return;
	}

	priv->disposed = TRUE;

	if (priv->notify_id) {
		g_source_remove (priv->notify_id);
		priv->notify_id = 0;
	}

	g_slist_foreach (priv->notify_props, (GFunc) g_free, NULL);
	g_slist_free (priv->notify_props);

	g_slist_foreach (priv->property_interfaces, (GFunc) g_free, NULL);
	g_slist_free (priv->property_interfaces);

	g_object_unref (priv->properties_proxy);
	dbus_g_connection_unref (priv->connection);

	G_OBJECT_CLASS (nm_object_parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
	NMObjectPrivate *priv = NM_OBJECT_GET_PRIVATE (object);

	g_slist_foreach (priv->property_tables, (GFunc) g_hash_table_destroy, NULL);
	g_slist_free (priv->property_tables);
	g_free (priv->path);

	G_OBJECT_CLASS (nm_object_parent_class)->finalize (object);
}

static void
set_property (GObject *object, guint prop_id,
			  const GValue *value, GParamSpec *pspec)
{
	NMObjectPrivate *priv = NM_OBJECT_GET_PRIVATE (object);
	DBusGConnection *connection;

	switch (prop_id) {
	case PROP_CONNECTION:
		/* Construct only */
		connection = (DBusGConnection *) g_value_get_boxed (value);
		if (!connection)
			connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, NULL);
		priv->connection = dbus_g_connection_ref (connection);
		break;
	case PROP_PATH:
		/* Construct only */
		priv->path = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
get_property (GObject *object, guint prop_id,
			  GValue *value, GParamSpec *pspec)
{
	NMObjectPrivate *priv = NM_OBJECT_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_CONNECTION:
		g_value_set_boxed (value, priv->connection);
		break;
	case PROP_PATH:
		g_value_set_string (value, priv->path);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nm_object_class_init (NMObjectClass *nm_object_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (nm_object_class);

	g_type_class_add_private (nm_object_class, sizeof (NMObjectPrivate));

	/* virtual methods */
	object_class->constructor = constructor;
	object_class->set_property = set_property;
	object_class->get_property = get_property;
	object_class->dispose = dispose;
	object_class->finalize = finalize;

	/* porperties */

	/**
	 * NMObject:connection:
	 *
	 * The #DBusGConnection of the object.
	 **/
	g_object_class_install_property
		(object_class, PROP_CONNECTION,
		 g_param_spec_boxed (NM_OBJECT_DBUS_CONNECTION,
							 "Connection",
							 "Connection",
							 DBUS_TYPE_G_CONNECTION,
							 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	/**
	 * NMObject:path:
	 *
	 * The DBus object path.
	 **/
	g_object_class_install_property
		(object_class, PROP_PATH,
		 g_param_spec_string (NM_OBJECT_DBUS_PATH,
							  "Object Path",
							  "DBus Object Path",
							  NULL,
							  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

/**
 * nm_object_get_connection:
 * @object: a #NMObject
 *
 * Gets the #NMObject's DBusGConnection.
 *
 * Returns: (transfer none): the connection
 **/
DBusGConnection *
nm_object_get_connection (NMObject *object)
{
	g_return_val_if_fail (NM_IS_OBJECT (object), NULL);

	return NM_OBJECT_GET_PRIVATE (object)->connection;
}

/**
 * nm_object_get_path:
 * @object: a #NMObject
 *
 * Gets the DBus path of the #NMObject.
 *
 * Returns: the object's path. This is the internal string used by the
 * device, and must not be modified.
 **/
const char *
nm_object_get_path (NMObject *object)
{
	g_return_val_if_fail (NM_IS_OBJECT (object), NULL);

	return NM_OBJECT_GET_PRIVATE (object)->path;
}

static gboolean
deferred_notify_cb (gpointer data)
{
	NMObject *object = NM_OBJECT (data);
	NMObjectPrivate *priv = NM_OBJECT_GET_PRIVATE (object);
	GSList *props, *iter;

	priv->notify_id = 0;

	/* Clear priv->notify_props early so that an NMObject subclass that
	 * listens to property changes can queue up other property changes
	 * during the g_object_notify() call separately from the property
	 * list we're iterating.
	 */
	props = g_slist_reverse (priv->notify_props);
	priv->notify_props = NULL;

	for (iter = props; iter; iter = g_slist_next (iter)) {
		g_object_notify (G_OBJECT (object), (const char *) iter->data);
		g_free (iter->data);
	}
	g_slist_free (props);
	return FALSE;
}

void
_nm_object_queue_notify (NMObject *object, const char *property)
{
	NMObjectPrivate *priv;
	gboolean found = FALSE;
	GSList *iter;

	g_return_if_fail (NM_IS_OBJECT (object));
	g_return_if_fail (property != NULL);

	priv = NM_OBJECT_GET_PRIVATE (object);
	if (!priv->notify_id)
		priv->notify_id = g_idle_add_full (G_PRIORITY_LOW, deferred_notify_cb, object, NULL);

	for (iter = priv->notify_props; iter; iter = g_slist_next (iter)) {
		if (!strcmp ((char *) iter->data, property)) {
			found = TRUE;
			break;
		}
	}

	if (!found)
		priv->notify_props = g_slist_prepend (priv->notify_props, g_strdup (property));
}

/* Stolen from dbus-glib */
static char*
wincaps_to_dash (const char *caps)
{
	const char *p;
	GString *str;

	str = g_string_new (NULL);
	p = caps;
	while (*p) {
		if (g_ascii_isupper (*p)) {
			if (str->len > 0 && (str->len < 2 || str->str[str->len-2] != '-'))
				g_string_append_c (str, '-');
			g_string_append_c (str, g_ascii_tolower (*p));
		} else
			g_string_append_c (str, *p);
		++p;
	}

	return g_string_free (str, FALSE);
}

static void
handle_property_changed (gpointer key, gpointer data, gpointer user_data)
{
	NMObject *self = NM_OBJECT (user_data);
	NMObjectPrivate *priv = NM_OBJECT_GET_PRIVATE (self);
	char *prop_name;
	PropertyInfo *pi;
	GParamSpec *pspec;
	gboolean success = FALSE, found = FALSE;
	GSList *iter;
	GValue *value = data;

	prop_name = wincaps_to_dash ((char *) key);

	/* Iterate through the object and its parents to find the property */
	for (iter = priv->property_tables; iter; iter = g_slist_next (iter)) {
		pi = g_hash_table_lookup ((GHashTable *) iter->data, prop_name);
		if (pi) {
			if (!pi->field) {
				/* We know about this property but aren't tracking changes on it. */
				goto out;
			}

			found = TRUE;
			break;
		}
	}

	if (!found) {
#if DEBUG
		g_warning ("Property '%s' unhandled.", prop_name);
#endif
		goto out;
	}

	pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (G_OBJECT (self)), prop_name);
	if (!pspec) {
		g_warning ("%s: property '%s' changed but wasn't defined by object type %s.",
		           __func__,
		           prop_name,
		           G_OBJECT_TYPE_NAME (self));
		goto out;
	}

	/* Handle NULL object paths */
	if (G_VALUE_HOLDS (value, DBUS_TYPE_G_OBJECT_PATH)) {
		if (g_strcmp0 (g_value_get_boxed (value), "/") == 0)
			value = NULL;
	}

	success = (*(pi->func)) (self, pspec, value, pi->field);
	if (!success) {
		g_warning ("%s: failed to update property '%s' of object type %s.",
		           __func__,
		           prop_name,
		           G_OBJECT_TYPE_NAME (self));
	}

out:
	g_free (prop_name);
}

void
_nm_object_process_properties_changed (NMObject *self, GHashTable *properties)
{
	g_hash_table_foreach (properties, handle_property_changed, self);
}

static void
properties_changed_proxy (DBusGProxy *proxy,
                          GHashTable *properties,
                          gpointer user_data)
{
	_nm_object_process_properties_changed (NM_OBJECT (user_data), properties);
}

#define HANDLE_TYPE(ucase, lcase, getter) \
	} else if (pspec->value_type == G_TYPE_##ucase) { \
		if (G_VALUE_HOLDS_##ucase (value)) { \
			g##lcase *param = (g##lcase *) field; \
			*param = g_value_get_##getter (value); \
		} else { \
			success = FALSE; \
			goto done; \
		}

static gboolean
demarshal_generic (NMObject *object,
                   GParamSpec *pspec,
                   GValue *value,
                   gpointer field)
{
	gboolean success = TRUE;

	if (pspec->value_type == G_TYPE_STRING) {
		if (G_VALUE_HOLDS_STRING (value)) {
			char **param = (char **) field;
			g_free (*param);
			*param = g_value_dup_string (value);
		} else if (G_VALUE_HOLDS (value, DBUS_TYPE_G_OBJECT_PATH)) {
			char **param = (char **) field;
			g_free (*param);
			*param = g_strdup (g_value_get_boxed (value));
			/* Handle "NULL" object paths */
			if (g_strcmp0 (*param, "/") == 0) {
				g_free (*param);
				*param = NULL;
			}
		} else {
			success = FALSE;
			goto done;
		}
	HANDLE_TYPE(BOOLEAN, boolean, boolean)
	HANDLE_TYPE(CHAR, char, schar)
	HANDLE_TYPE(UCHAR, uchar, uchar)
	HANDLE_TYPE(DOUBLE, double, double)
	HANDLE_TYPE(INT, int, int)
	HANDLE_TYPE(UINT, uint, uint)
	HANDLE_TYPE(INT64, int, int)
	HANDLE_TYPE(UINT64, uint, uint)
	HANDLE_TYPE(LONG, long, long)
	HANDLE_TYPE(ULONG, ulong, ulong)
	} else {
		g_warning ("%s: %s/%s unhandled type %s.",
		           __func__, G_OBJECT_TYPE_NAME (object), pspec->name,
		           g_type_name (pspec->value_type));
		success = FALSE;
	}

done:
	if (success) {
		_nm_object_queue_notify (object, pspec->name);
	} else {
		g_warning ("%s: %s/%s (type %s) couldn't be set with type %s.",
		           __func__, G_OBJECT_TYPE_NAME (object), pspec->name,
		           g_type_name (pspec->value_type), G_VALUE_TYPE_NAME (value));
	}
	return success;
}

void
_nm_object_register_properties (NMObject *object,
                                DBusGProxy *proxy,
                                const NMPropertiesInfo *info)
{
	NMObjectPrivate *priv = NM_OBJECT_GET_PRIVATE (object);
	NMPropertiesInfo *tmp;
	GHashTable *instance;

	g_return_if_fail (NM_IS_OBJECT (object));
	g_return_if_fail (proxy != NULL);
	g_return_if_fail (info != NULL);

	priv->property_interfaces = g_slist_prepend (priv->property_interfaces,
	                                             g_strdup (dbus_g_proxy_get_interface (proxy)));

	dbus_g_proxy_add_signal (proxy, "PropertiesChanged", DBUS_TYPE_G_MAP_OF_VARIANT, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy,
						    "PropertiesChanged",
						    G_CALLBACK (properties_changed_proxy),
						    object,
						    NULL);

	instance = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	priv->property_tables = g_slist_prepend (priv->property_tables, instance);

	for (tmp = (NMPropertiesInfo *) info; tmp->name; tmp++) {
		PropertyInfo *pi;

		if (!tmp->name || (tmp->func && !tmp->field)) {
			g_warning ("%s: missing field in NMPropertiesInfo", __func__);
			continue;
		}

		pi = g_malloc0 (sizeof (PropertyInfo));
		pi->func = tmp->func ? tmp->func : demarshal_generic;
		pi->field = tmp->field;
		g_hash_table_insert (instance, g_strdup (tmp->name), pi);
	}
}

gboolean
_nm_object_reload_properties (NMObject *object, GError **error)
{
	NMObjectPrivate *priv = NM_OBJECT_GET_PRIVATE (object);
	GHashTable *props = NULL;
	GSList *p;

	if (!priv->property_interfaces)
		return TRUE;

	priv->inited = TRUE;

	for (p = priv->property_interfaces; p; p = p->next) {
		if (!dbus_g_proxy_call (priv->properties_proxy, "GetAll", error,
		                        G_TYPE_STRING, p->data,
		                        G_TYPE_INVALID,
		                        DBUS_TYPE_G_MAP_OF_VARIANT, &props,
		                        G_TYPE_INVALID))
			return FALSE;

		_nm_object_process_properties_changed (object, props);
		g_hash_table_destroy (props);
	}

	return TRUE;
}

void
_nm_object_ensure_inited (NMObject *object)
{
	NMObjectPrivate *priv = NM_OBJECT_GET_PRIVATE (object);
	GError *error = NULL;

	if (!priv->inited) {
		if (!_nm_object_reload_properties (object, &error)) {
			g_warning ("Could not initialize %s %s: %s",
			           G_OBJECT_TYPE_NAME (object), priv->path,
			           error->message);
			g_error_free (error);
		}
	}
}

void
_nm_object_reload_property (NMObject *object,
                            const char *interface,
                            const char *prop_name)
{
	GValue value = { 0, };
	GError *err = NULL;

	g_return_if_fail (NM_IS_OBJECT (object));
	g_return_if_fail (interface != NULL);
	g_return_if_fail (prop_name != NULL);

	if (!dbus_g_proxy_call_with_timeout (NM_OBJECT_GET_PRIVATE (object)->properties_proxy,
							"Get", 15000, &err,
							G_TYPE_STRING, interface,
							G_TYPE_STRING, prop_name,
							G_TYPE_INVALID,
							G_TYPE_VALUE, &value,
							G_TYPE_INVALID)) {
		/* Don't warn about D-Bus no reply/timeout errors; it's mostly noise and
		 * happens for example when NM quits and the applet is still running.
		 */
		if (!g_error_matches (err, DBUS_GERROR, DBUS_GERROR_NO_REPLY)) {
			g_warning ("%s: Error getting '%s' for %s: (%d) %s\n",
			           __func__,
			           prop_name,
			           nm_object_get_path (object),
			           err->code,
			           err->message);
		}
		g_clear_error (&err);
		return;
	}

	handle_property_changed ((gpointer)prop_name, &value, object);
	g_value_unset (&value);
}

void
_nm_object_set_property (NMObject *object,
						const char *interface,
						const char *prop_name,
						GValue *value)
{
	g_return_if_fail (NM_IS_OBJECT (object));
	g_return_if_fail (interface != NULL);
	g_return_if_fail (prop_name != NULL);
	g_return_if_fail (G_IS_VALUE (value));

	if (!dbus_g_proxy_call_with_timeout (NM_OBJECT_GET_PRIVATE (object)->properties_proxy,
	                                     "Set", 2000, NULL,
	                                     G_TYPE_STRING, interface,
	                                     G_TYPE_STRING, prop_name,
	                                     G_TYPE_VALUE, value,
	                                     G_TYPE_INVALID)) {

		/* Ignore errors. dbus_g_proxy_call_with_timeout() is called instead of
		 * dbus_g_proxy_call_no_reply() to give NM chance to authenticate the caller.
		 */
	}
}
