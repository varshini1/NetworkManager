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
 * Copyright 2013 Red Hat, Inc.
 */

#include "config.h"

#include <string.h>
#include <netinet/ether.h>

#include "nm-test-device.h"
#include "nm-device-private.h"

static GObjectClass *g_object_class;

G_DEFINE_TYPE (NMTestDevice, nm_test_device, NM_TYPE_DEVICE)

static void
nm_test_device_init (NMTestDevice *self)
{
}

/* We jump over NMDevice's construct/destruct methods, which require NMPlatform
 * and NMConnectionProvider to be initialized.
 */

static GObject*
constructor (GType type,
             guint n_construct_params,
             GObjectConstructParam *construct_params)
{
	return g_object_class->constructor (type,
	                                    n_construct_params,
	                                    construct_params);
}

static void
constructed (GObject *object)
{
	NMDevice *device = NM_DEVICE (object);

	nm_device_update_hw_address (device);

	g_object_class->constructed (object);
}

static void
dispose (GObject *object)
{
	g_object_class->dispose (object);
}

static void
finalize (GObject *object)
{
	g_object_class->finalize (object);
}

static guint
get_hw_address_length (NMDevice *dev, gboolean *out_permanent)
{
	if (out_permanent)
		*out_permanent = TRUE;
	return ETH_ALEN;
}

static void
nm_test_device_class_init (NMTestDeviceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	NMDeviceClass *device_class = NM_DEVICE_CLASS (klass);

	g_object_class = g_type_class_peek (G_TYPE_OBJECT);

	object_class->constructor = constructor;
	object_class->constructed = constructed;
	object_class->dispose = dispose;
	object_class->finalize = finalize;

	device_class->get_hw_address_length = get_hw_address_length;
}

NMDevice *
nm_test_device_new (const char *hwaddr)
{
	return g_object_new (NM_TYPE_TEST_DEVICE,
	                     NM_DEVICE_IFACE, "dummy:",
	                     NM_DEVICE_HW_ADDRESS, hwaddr,
	                     NULL);
}
