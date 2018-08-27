 /*
 * bluetooth-shell.h
 *
 *  Created on: Aug 21, 2018
 *      Author: Liem Le
 */
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <glib.h>
#include "gdbus.h"
#include "mainloop.h"
#include "gatt.h"
#include "util.h"


#define PROMPT_ON	"Connected to bluetoothd!\n"
#define PROMPT_OFF	"Waiting to connect to bluetoothd...\n"

#define COLOR_OFF		"\x1B[0m"
#define COLOR_RED		"\x1B[0;91m"
#define COLOR_GREEN		"\x1B[0;92m"
#define COLOR_YELLOW	"\x1B[0;93m"
#define COLOR_BLUE		"\x1B[0;94m"
#define COLOR_BOLDGRAY	"\x1B[1;30m"
#define COLOR_BOLDWHITE	"\x1B[1;37m"

#define COLORED_NEW	COLOR_GREEN "ADD" COLOR_OFF
#define COLORED_CHG	COLOR_BLUE 	"CHG" COLOR_OFF
#define COLORED_DEL	COLOR_RED 	"DEL" COLOR_OFF

#define CURA8_SENSOR_NAME				"CURA8_PROTOTYPE"
#define CURA8_BASE_UUID					"d2abe200-da7f-4a65-8f16-7a96bb02ca81"
#define CURA8_TEMP_CHARACTERISTIC_UUID	"d2abe201-da7f-4a65-8f16-7a96bb02ca81"
#define CURA8_CMD_CHARACTERISTIC_UUID	"d2abe202-da7f-4a65-8f16-7a96bb02ca81"

#define CURA8_READER_SEC_KEY            "Cura8.reader"


struct adapter {
	GDBusProxy *proxy;
	GDBusProxy *ad_proxy;
	GList *devices;
};

typedef enum{
	APP_SUCCESS,
	APP_DEV_NOT_AVAILABLE,
	APP_UNKOWN,
}APP_RET_CODE;


extern bool clear_devices, scan_flag, scanning, attempting_quit;

extern DBusConnection *dbus_conn;
extern GDBusProxy *agent_manager;
extern char *auto_register_agent;
extern struct adapter *default_ctrl;
extern GDBusProxy *default_dev;
extern GDBusProxy *default_attr;
extern GList *ctrl_list;

void bluetooth_shell_init(void);
void bluetooth_shell_destruct(void);

void scan(bool enable);
APP_RET_CODE print_info(const char *address);
void cura8_attempting_to_connect(const char *address);
void cura8_temperature_notify(dbus_bool_t enable);

typedef enum{
	CURA8_SEC,
	CURA8_ADC_SRC_SEL,
	CURA8_UPDATE_INTERVAL,
}CURA8_CMD_TYPE_t;

void cura8_command_write(CURA8_CMD_TYPE_t type, char *data, size_t len);
// static void connect(const char *address);
// static void disconnect(const char *address);
void remove_device(const char *address);
void remove_all_devices(void);

void device_added_callback(const char *name, const char *address);
void cura8_received_data_callback(unsigned char *byte, int *len);

void found_cura8_device_callback(const char *name, const char *address);
void connection_successful_callback(void);