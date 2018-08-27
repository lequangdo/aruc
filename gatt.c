/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2014  Intel Corporation. All rights reserved.
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <string.h>

#include <glib.h>

#include "util.h"
#include "io.h"
#include "gdbus.h"
#include "gatt.h"

#define APP_PATH "/org/bluez/app"
#define PROFILE_INTERFACE "org.bluez.GattProfile1"
#define SERVICE_INTERFACE "org.bluez.GattService1"
#define CHRC_INTERFACE "org.bluez.GattCharacteristic1"
#define DESC_INTERFACE "org.bluez.GattDescriptor1"

/* String display constants */
#define COLOR_OFF		"\x1B[0m"
#define COLOR_RED		"\x1B[0;91m"
#define COLOR_GREEN		"\x1B[0;92m"
#define COLOR_YELLOW	"\x1B[0;93m"
#define COLOR_BLUE		"\x1B[0;94m"
#define COLOR_BOLDGRAY	"\x1B[1;30m"
#define COLOR_BOLDWHITE	"\x1B[1;37m"

#define COLORED_NEW	COLOR_GREEN "NEW" COLOR_OFF
#define COLORED_CHG	COLOR_YELLOW "CHG" COLOR_OFF
#define COLORED_DEL	COLOR_RED "DEL" COLOR_OFF

bool write_done = false;

struct desc {
	struct chrc *chrc;
	char *path;
	char *uuid;
	char **flags;
	int value_len;
	unsigned int max_val_len;
	uint8_t *value;
};

struct chrc {
	struct service *service;
	char *path;
	char *uuid;
	char **flags;
	bool notifying;
	GList *descs;
	int value_len;
	unsigned int max_val_len;
	uint8_t *value;
	uint16_t mtu;
	struct io *write_io;
	struct io *notify_io;
	bool authorization_req;
};

struct service {
	DBusConnection *conn;
	char *path;
	char *uuid;
	bool primary;
	GList *chrcs;
	GList *inc;
};

static GList *local_services;
static GList *services;
static GList *characteristics;
static GList *descriptors;
static GList *managers;
static GList *uuids;
static DBusMessage *pending_message = NULL;

struct pipe_io {
	GDBusProxy *proxy;
	struct io *io;
	uint16_t mtu;
};

static struct pipe_io write_io;
static struct pipe_io notify_io;

void gatt_reset(void){
	g_list_free(local_services);
	g_list_free(services);
	g_list_free(characteristics);
	g_list_free(descriptors);
	g_list_free(managers);
	g_list_free(uuids);
}

static void print_service(struct service *service, const char *description)
{
	const char *text;

	text = bt_uuidstr_to_str(service->uuid);
	if (!text)
		printf("%s%s%s%s Service\n\t%s\n\t%s\n",
					description ? "[" : "",
					description ? : "",
					description ? "] " : "",
					service->primary ? "Primary" :
					"Secondary",
					service->path, service->uuid);
	else
		printf("%s%s%s%s Service\n\t%s\n\t%s\n\t%s\n",
					description ? "[" : "",
					description ? : "",
					description ? "] " : "",
					service->primary ? "Primary" :
					"Secondary",
					service->path, service->uuid, text);
}

static void print_inc_service(struct service *service, const char *description)
{
	const char *text;

	text = bt_uuidstr_to_str(service->uuid);
	if (!text)
		printf("%s%s%s%s Included Service\n\t%s\n\t%s\n",
					description ? "[" : "",
					description ? : "",
					description ? "] " : "",
					service->primary ? "Primary" :
					"Secondary",
					service->path, service->uuid);
	else
		printf("%s%s%s%s Included Service\n\t%s\n\t%s\n\t%s\n",
					description ? "[" : "",
					description ? : "",
					description ? "] " : "",
					service->primary ? "Primary" :
					"Secondary",
					service->path, service->uuid, text);
}

static void print_service_proxy(GDBusProxy *proxy, const char *description)
{
	struct service service;
	DBusMessageIter iter;
	const char *uuid;
	dbus_bool_t primary;

	if (g_dbus_proxy_get_property(proxy, "UUID", &iter) == FALSE)
		return;

	dbus_message_iter_get_basic(&iter, &uuid);

	if (g_dbus_proxy_get_property(proxy, "Primary", &iter) == FALSE)
		return;

	dbus_message_iter_get_basic(&iter, &primary);

	service.path = (char *) g_dbus_proxy_get_path(proxy);
	service.uuid = (char *) uuid;
	service.primary = primary;

	print_service(&service, description);
}

void gatt_add_service(GDBusProxy *proxy)
{
	services = g_list_append(services, proxy);

	print_service_proxy(proxy, COLORED_NEW);
}

void gatt_remove_service(GDBusProxy *proxy)
{
	GList *l;

	l = g_list_find(services, proxy);
	if (!l)
		return;

	services = g_list_delete_link(services, l);

	print_service_proxy(proxy, COLORED_DEL);
}

static void print_chrc(struct chrc *chrc, const char *description)
{
	const char *text;

	text = bt_uuidstr_to_str(chrc->uuid);
	if (!text)
		printf("%s%s%sCharacteristic\n\t%s\n\t%s\n",
					description ? "[" : "",
					description ? : "",
					description ? "] " : "",
					chrc->path, chrc->uuid);
	else
		printf("%s%s%sCharacteristic\n\t%s\n\t%s\n\t%s\n",
					description ? "[" : "",
					description ? : "",
					description ? "] " : "",
					chrc->path, chrc->uuid, text);
}

static void print_characteristic(GDBusProxy *proxy, const char *description)
{
	struct chrc chrc;
	DBusMessageIter iter;
	const char *uuid;

	

	if (g_dbus_proxy_get_property(proxy, "UUID", &iter) == FALSE)
		return;

	dbus_message_iter_get_basic(&iter, &uuid);

	chrc.path = (char *) g_dbus_proxy_get_path(proxy);
	chrc.uuid = (char *) uuid;

	print_chrc(&chrc, description);
}

static gboolean chrc_is_child(GDBusProxy *characteristic)
{
	DBusMessageIter iter;
	const char *service;
	
	if (!g_dbus_proxy_get_property(characteristic, "Service", &iter))
		return FALSE;

	dbus_message_iter_get_basic(&iter, &service);

	return g_dbus_proxy_lookup(services, NULL, service,
					"org.bluez.GattService1") != NULL;
}

void gatt_add_characteristic(GDBusProxy *proxy)
{
	
	if (!chrc_is_child(proxy))
		return;

	characteristics = g_list_append(characteristics, proxy);

	print_characteristic(proxy, COLORED_NEW);
}

static void notify_io_destroy(void)
{
	io_destroy(notify_io.io);
	memset(&notify_io, 0, sizeof(notify_io));
}

static void write_io_destroy(void)
{
	io_destroy(write_io.io);
	memset(&write_io, 0, sizeof(write_io));
}

void gatt_remove_characteristic(GDBusProxy *proxy)
{
	GList *l;

	l = g_list_find(characteristics, proxy);
	if (!l)
		return;

	characteristics = g_list_delete_link(characteristics, l);

	print_characteristic(proxy, COLORED_DEL);

	if (write_io.proxy == proxy)
		write_io_destroy();
	else if (notify_io.proxy == proxy)
		notify_io_destroy();
}

static void print_desc(struct desc *desc, const char *description)
{
	const char *text;

	text = bt_uuidstr_to_str(desc->uuid);
	if (!text)
		printf("%s%s%sDescriptor\n\t%s\n\t%s\n",
					description ? "[" : "",
					description ? : "",
					description ? "] " : "",
					desc->path, desc->uuid);
	else
		printf("%s%s%sDescriptor\n\t%s\n\t%s\n\t%s\n",
					description ? "[" : "",
					description ? : "",
					description ? "] " : "",
					desc->path, desc->uuid, text);
}

static void print_descriptor(GDBusProxy *proxy, const char *description)
{
	struct desc desc;
	DBusMessageIter iter;
	const char *uuid;

	if (g_dbus_proxy_get_property(proxy, "UUID", &iter) == FALSE)
		return;

	dbus_message_iter_get_basic(&iter, &uuid);

	desc.path = (char *) g_dbus_proxy_get_path(proxy);
	desc.uuid = (char *) uuid;

	print_desc(&desc, description);
}

static gboolean descriptor_is_child(GDBusProxy *characteristic)
{
	GList *l;
	DBusMessageIter iter;
	const char *service, *path;

	if (!g_dbus_proxy_get_property(characteristic, "Characteristic", &iter))
		return FALSE;

	dbus_message_iter_get_basic(&iter, &service);

	for (l = characteristics; l; l = g_list_next(l)) {
		GDBusProxy *proxy = l->data;

		path = g_dbus_proxy_get_path(proxy);

		if (!strcmp(path, service))
			return TRUE;
	}

	return FALSE;
}

void gatt_add_descriptor(GDBusProxy *proxy)
{
	if (!descriptor_is_child(proxy))
		return;

	descriptors = g_list_append(descriptors, proxy);

	print_descriptor(proxy, COLORED_NEW);
}

void gatt_remove_descriptor(GDBusProxy *proxy)
{
	GList *l;

	l = g_list_find(descriptors, proxy);
	if (!l)
		return;

	descriptors = g_list_delete_link(descriptors, l);

	print_descriptor(proxy, COLORED_DEL);
}

static GDBusProxy *select_attribute(const char *path)
{
	GDBusProxy *proxy;

	proxy = g_dbus_proxy_lookup(services, NULL, path,
					"org.bluez.GattService1");
	if (proxy)
		return proxy;

	proxy = g_dbus_proxy_lookup(characteristics, NULL, path,
					"org.bluez.GattCharacteristic1");
	if (proxy)
		return proxy;

	return g_dbus_proxy_lookup(descriptors, NULL, path,
					"org.bluez.GattDescriptor1");
}

static GDBusProxy *select_proxy_by_uuid(GDBusProxy *parent, const char *uuid,
					GList *source)
{
	GList *l;
	const char *value;
	DBusMessageIter iter;

	for (l = source; l; l = g_list_next(l)) {
		GDBusProxy *proxy = l->data;

		if (parent && !g_str_has_prefix(g_dbus_proxy_get_path(proxy),
						g_dbus_proxy_get_path(parent)))
			continue;

		if (g_dbus_proxy_get_property(proxy, "UUID", &iter) == FALSE)
			continue;

		dbus_message_iter_get_basic(&iter, &value);

		if (strcasecmp(uuid, value) == 0)
			return proxy;
	}

	return NULL;
}

static GDBusProxy *select_attribute_by_uuid(GDBusProxy *parent,
							const char *uuid)
{
	GDBusProxy *proxy;

	proxy = select_proxy_by_uuid(parent, uuid, services);
	if (proxy)
		return proxy;

	proxy = select_proxy_by_uuid(parent, uuid, characteristics);
	if (proxy)
		return proxy;

	return select_proxy_by_uuid(parent, uuid, descriptors);
}

GDBusProxy *gatt_select_attribute(GDBusProxy *parent, const char *arg)
{
	// if (arg[0] == '/')
	// 	return select_attribute(arg);

	if (parent) {
		GDBusProxy *proxy = select_attribute_by_uuid(parent, arg);
		if (proxy)
			return proxy;
	}
	return select_attribute_by_uuid(parent, arg);
}


static void notify_reply(DBusMessage *message, void *user_data)
{
	bool enable = GPOINTER_TO_UINT(user_data);
	DBusError error;

	dbus_error_init(&error);

	if (dbus_set_error_from_message(&error, message) == TRUE) {
		printf("Failed to %s notify: %s\n",
				enable ? "start" : "stop", error.name);
		dbus_error_free(&error);
		return;
	}

	printf("Notify %s\n", enable == TRUE ? "started" : "stopped");

	return;
}

static void notify_attribute(GDBusProxy *proxy, bool enable)
{
	const char *method;

	if (enable == TRUE)
		method = "StartNotify";
	else
		method = "StopNotify";

	if (g_dbus_proxy_method_call(proxy, method, NULL, notify_reply,
				GUINT_TO_POINTER(enable), NULL) == FALSE) {
		printf("Failed to %s notify\n",
				enable ? "start" : "stop");
		return;
	}

	return;
}

void gatt_notify_attribute(GDBusProxy *proxy, bool enable)
{
	const char *iface;

	iface = g_dbus_proxy_get_interface(proxy);
	if (!strcmp(iface, "org.bluez.GattCharacteristic1")) {
		notify_attribute(proxy, enable);
		return;
	}

	printf("Unable to notify attribute %s\n",
						g_dbus_proxy_get_path(proxy));

	return;
}

static void write_reply(DBusMessage *message, void *user_data)
{
	DBusError error;

	dbus_error_init(&error);

	if (dbus_set_error_from_message(&error, message) == TRUE) {
		printf("Failed to write: %s\n", error.name);
		dbus_error_free(&error);
		return;// bt_shell_noninteractive_quit(EXIT_FAILURE);
	}
	write_done = true;
	printf("Successed to write!\n");
	return;// bt_shell_noninteractive_quit(EXIT_SUCCESS);
}

struct write_attribute_data {
	struct iovec *iov;
	uint16_t offset;
};

static void write_setup(DBusMessageIter *iter, void *user_data)
{
	struct write_attribute_data *wd = user_data;
	DBusMessageIter array, dict;

	dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, "y", &array);
	dbus_message_iter_append_fixed_array(&array, DBUS_TYPE_BYTE,
						&wd->iov->iov_base,
						wd->iov->iov_len);
	dbus_message_iter_close_container(iter, &array);

	dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY,
					DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
					DBUS_TYPE_STRING_AS_STRING
					DBUS_TYPE_VARIANT_AS_STRING
					DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
					&dict);

	g_dbus_dict_append_entry(&dict, "offset", DBUS_TYPE_UINT16,
								&wd->offset);

	dbus_message_iter_close_container(iter, &dict);
}

static void write_attribute(GDBusProxy *proxy, uint16_t offset, char *val_str, size_t len)
{
	struct iovec iov;
	struct write_attribute_data wd;
	// uint8_t value[MAX_ATTR_VAL_LEN];
	char *entry;
	// unsigned int i;

	// memcpy(value, val_str , len);

	iov.iov_base = val_str;
	iov.iov_len = len;

	/* Write using the fd if it has been acquired and fit the MTU */
	if (proxy == write_io.proxy && (write_io.io && write_io.mtu >= len)) {
		printf("Attempting to write fd %d\n",
						io_get_fd(write_io.io));
		if (io_send(write_io.io, &iov, 1) < 0) {
			printf("Failed to write: %s", strerror(errno));
			return;// bt_shell_noninteractive_quit(EXIT_FAILURE);
		}
		return;
	}

	wd.iov = &iov;
	wd.offset = offset;

	write_done = false;

	if (g_dbus_proxy_method_call(proxy, "WriteValue", write_setup,
					write_reply, &wd, NULL) == FALSE) {
		printf("Failed to write\n");
		return;// bt_shell_noninteractive_quit(EXIT_FAILURE);
	}

	printf("Attempting to write %s\n",
					g_dbus_proxy_get_path(proxy));
}

void gatt_write_attribute(GDBusProxy *proxy, uint16_t offset, char *data, size_t len)
{
	const char *iface;
	// uint16_t offset = 0;

	iface = g_dbus_proxy_get_interface(proxy);
	if (!strcmp(iface, "org.bluez.GattCharacteristic1") ||
				!strcmp(iface, "org.bluez.GattDescriptor1")) {

		// if (argc > 2)
		// 	offset = atoi(argv[2]);

		write_attribute(proxy, offset, data, len);
		return;
	}

	printf("Unable to write attribute %s\n",
						g_dbus_proxy_get_path(proxy));

	// return bt_shell_noninteractive_quit(EXIT_FAILURE);
}