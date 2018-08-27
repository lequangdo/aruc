 /*
 * bluetooth-shell.h
 *
 *  Created on: Aug 21, 2018
 *      Author: Liem Le
 */
#include "bluetooth-shell.h"

bool clear_devices = FALSE, scan_flag = TRUE, scanning, attempting_quit =FALSE;
DBusConnection *dbus_conn;
GDBusProxy *agent_manager;
char *auto_register_agent = NULL;
struct adapter *default_ctrl;
GDBusProxy *default_dev;
GDBusProxy *default_attr;
GDBusProxy *temp_attr, *cmd_attr;
GList *ctrl_list;

GDBusClient *client;


/* Client handlers */
static void proxy_leak(gpointer data);
static void connect_handler(DBusConnection *connection, void *user_data);
static void disconnect_handler(DBusConnection *connection, void *user_data);
static void message_handler(DBusConnection *connection,	DBusMessage *message, void *user_data);
static void proxy_added_handler(GDBusProxy *proxy, void *user_data);
static void proxy_removed_handler(GDBusProxy *proxy, void *user_data);
static void property_changed_handler(GDBusProxy *proxy, const char *name, DBusMessageIter *iter, void *user_data);

void bluetooth_shell_init(void){
    

    dbus_conn = g_dbus_setup_bus(DBUS_BUS_SYSTEM, NULL, NULL);
	g_dbus_attach_object_manager(dbus_conn);

	client = g_dbus_client_new(dbus_conn, "org.bluez", "/org/bluez");
	if(client == NULL){
		printf("Can not create dbus client! \n");
	}
	else{
		printf("client: 0x%x\n",client);
	}

	g_dbus_client_set_connect_watch(client, connect_handler, NULL);
	g_dbus_client_set_disconnect_watch(client, disconnect_handler, NULL);
	g_dbus_client_set_signal_watch(client, message_handler, NULL);
	g_dbus_client_set_proxy_handlers(client, 
                                    proxy_added_handler, 
                                    proxy_removed_handler, 
                                    property_changed_handler, 
                                    NULL);
	g_dbus_client_set_ready_watch(client, NULL, NULL);
} 

void bluetooth_shell_destruct(void){
    g_dbus_client_unref(client);

	dbus_connection_unref(dbus_conn);

	g_list_free_full(ctrl_list, proxy_leak);

	g_free(auto_register_agent);
	
	printf("End!\n");
}

/* bluez adapter msg helper */
static void print_string(const char *str, void *user_data){
	printf("%s\n", str);
}

static void print_fixed_iter(const char *label, const char *name, DBusMessageIter *iter){
	dbus_bool_t *valbool;
	dbus_uint32_t *valu32;
	dbus_uint16_t *valu16;
	dbus_int16_t *vals16;
	unsigned char *byte;
	int len;

	switch (dbus_message_iter_get_arg_type(iter)) {
	case DBUS_TYPE_BOOLEAN:
		dbus_message_iter_get_fixed_array(iter, &valbool, &len);

		if (len <= 0)
			return;

		printf("%s%s:\n", label, name);
		util_hexdump(' ',(void *)valbool, len * sizeof(*valbool), print_string, NULL);

		break;
	case DBUS_TYPE_UINT32:
		dbus_message_iter_get_fixed_array(iter, &valu32, &len);

		if (len <= 0)
			return;

		printf("%s%s:\n", label, name);
		util_hexdump(' ',(void *)valu32, len * sizeof(*valu32), print_string, NULL);

		break;
	case DBUS_TYPE_UINT16:
		dbus_message_iter_get_fixed_array(iter, &valu16, &len);

		if (len <= 0)
			return;

		printf("%s%s:\n", label, name);
		util_hexdump(' ',(void *)valu16, len * sizeof(*valu16), print_string, NULL);

		break;
	case DBUS_TYPE_INT16:
		dbus_message_iter_get_fixed_array(iter, &vals16, &len);

		if (len <= 0)
			return;

		printf("%s%s:\n", label, name);
		util_hexdump(' ',(void *)vals16, len * sizeof(*vals16), print_string, NULL);

		break;
	case DBUS_TYPE_BYTE:
		dbus_message_iter_get_fixed_array(iter, &byte, &len);

		if (len <= 0)
			return;

		if(temp_attr){
			cura8_received_data_callback(byte,&len);				
		}				
		else{
			printf("%s%s:\n", label, name);
			util_hexdump(' ',(void *)byte, len * sizeof(*byte), print_string, NULL);
		}
		break;
	default:
		return;
	};
}

static void print_iter(const char *label, const char *name, DBusMessageIter *iter){
	dbus_bool_t valbool;
	dbus_uint32_t valu32;
	dbus_uint16_t valu16;
	dbus_int16_t vals16;
	unsigned char byte;
	const char *valstr;
	DBusMessageIter subiter;
	char *entry;

	if (iter == NULL) {
		printf("%s%s is nil\n", label, name);
		return;
	}

	switch (dbus_message_iter_get_arg_type(iter)) {
	case DBUS_TYPE_INVALID:
		printf("%s%s is invalid\n", label, name);
		break;
	case DBUS_TYPE_STRING:
	case DBUS_TYPE_OBJECT_PATH:
		dbus_message_iter_get_basic(iter, &valstr);
		printf("%s%s:\t%s%s\n", label, name,strlen(name)<=strlen("Device")?"\t":"", valstr);
		break;
	case DBUS_TYPE_BOOLEAN:
		dbus_message_iter_get_basic(iter, &valbool);
		printf("%s%s:\t%s%s\n", label, name,strlen(name)<=strlen("Device")?"\t":"",
					valbool == TRUE ? "yes" : "no");
		break;
	case DBUS_TYPE_UINT32:
		dbus_message_iter_get_basic(iter, &valu32);
		printf("%s%s: 0x%08x\n", label, name, valu32);
		break;
	case DBUS_TYPE_UINT16:
		dbus_message_iter_get_basic(iter, &valu16);
		printf("%s%s: 0x%04x\n", label, name, valu16);
		break;
	case DBUS_TYPE_INT16:
		dbus_message_iter_get_basic(iter, &vals16);
		printf("%s%s: %d\n", label, name, vals16);
		break;
	case DBUS_TYPE_BYTE:
		dbus_message_iter_get_basic(iter, &byte);
		printf("%s%s: 0x%02x\n", label, name, byte);
		break;
	case DBUS_TYPE_VARIANT:
		dbus_message_iter_recurse(iter, &subiter);
		print_iter(label, name, &subiter);
		break;
	case DBUS_TYPE_ARRAY:
		dbus_message_iter_recurse(iter, &subiter);

		if (dbus_type_is_fixed(
				dbus_message_iter_get_arg_type(&subiter))) {
			print_fixed_iter(label, name, &subiter);
			break;
		}

		while (dbus_message_iter_get_arg_type(&subiter) !=
							DBUS_TYPE_INVALID) {
			print_iter(label, name, &subiter);
			dbus_message_iter_next(&subiter);
		}
		break;
	case DBUS_TYPE_DICT_ENTRY:
		dbus_message_iter_recurse(iter, &subiter);
		entry = g_strconcat(name, " Key", NULL);
		print_iter(label, entry, &subiter);
		g_free(entry);

		entry = g_strconcat(name, " Value", NULL);
		dbus_message_iter_next(&subiter);
		print_iter(label, entry, &subiter);
		g_free(entry);
		break;
	default:
		printf("%s%s has unsupported type\n", label, name);
		break;
	}
}

static struct adapter *find_ctrl(GList *source, const char *path){
	GList *list;

	for (list = g_list_first(source); list; list = g_list_next(list)) {
		struct adapter *adapter = list->data;

		if (!strcasecmp(g_dbus_proxy_get_path(adapter->proxy), path))
			return adapter;
	}

	return NULL;
}

static void print_adapter(GDBusProxy *proxy, const char *description){
	DBusMessageIter iter;
	const char *address, *name;	

	if (g_dbus_proxy_get_property(proxy, "Address", &iter) == FALSE){
		printf("proxy_get_property: failed!");
		return;
	}		

	dbus_message_iter_get_basic(&iter, &address);

	if (g_dbus_proxy_get_property(proxy, "Alias", &iter) == TRUE)
		dbus_message_iter_get_basic(&iter, &name);
	else
		name = "<unknown>";

	printf("%s%s%sController %s %s %s\n",
				description ? "[" : "",
				description ? : "",
				description ? "] " : "",
				address, name,
				default_ctrl &&	default_ctrl->proxy == proxy ? "[default]" : "");

}

static void list_addapter(void){
	GList *list;

	for (list = g_list_first(ctrl_list); list; list = g_list_next(list)) {
		printf("\nFound controller!\n");
		struct adapter *adapter = list->data;
		print_adapter(adapter->proxy, NULL);
	}

	// quit();
}

static struct adapter *adapter_new(GDBusProxy *proxy){
	struct adapter *adapter = g_malloc0(sizeof(struct adapter));
	printf("adapter_new\n");
	ctrl_list = g_list_append(ctrl_list, adapter);

	if (!default_ctrl)
		default_ctrl = adapter;

	return adapter;
}

static void adapter_added(GDBusProxy *proxy){
	struct adapter *adapter;
	printf("adapter_added\n");
	adapter = find_ctrl(ctrl_list, g_dbus_proxy_get_path(proxy));
	if (!adapter)
		adapter = adapter_new(proxy);

	adapter->proxy = proxy;

	print_adapter(proxy, NULL);
	// remove_device("B8:27:EB:D3:F5:CF");
	if(adapter && scan_flag){		
		// scan(TRUE);
		// mainloop_quit();
		//list_controller();
		// quit();
		scan_flag = FALSE;
	}
}

/* bluez device msg helper */
static void set_default_device(GDBusProxy *proxy, const char *attribute){
	char *desc = NULL;
	DBusMessageIter iter;
	const char *path;

	default_dev = proxy;

	if (proxy == NULL) {
		default_attr = NULL;
		goto done;
	}

	if (!g_dbus_proxy_get_property(proxy, "Alias", &iter)) {
		if (!g_dbus_proxy_get_property(proxy, "Address", &iter))
			goto done;
	}

	path = g_dbus_proxy_get_path(proxy);

	dbus_message_iter_get_basic(&iter, &desc);
	desc = g_strdup_printf(COLOR_BLUE "[%s%s%s]" COLOR_OFF "# ", desc,
				attribute ? ":" : "",
				attribute ? attribute + strlen(path) : "");

done:
	g_free(desc);
}

static gboolean device_is_child(GDBusProxy *device, GDBusProxy *master){
	DBusMessageIter iter;
	const char *adapter, *path;

	if (!master)
		return FALSE;

	if (g_dbus_proxy_get_property(device, "Adapter", &iter) == FALSE)
		return FALSE;

	dbus_message_iter_get_basic(&iter, &adapter);
	path = g_dbus_proxy_get_path(master);

	if (!strcmp(path, adapter))
		return TRUE;

	return FALSE;
}

static struct adapter *find_parent(GDBusProxy *device){
	GList *list;

	for (list = g_list_first(ctrl_list); list; list = g_list_next(list)) {
		struct adapter *adapter = list->data;

		if (device_is_child(device, adapter->proxy) == TRUE)
			return adapter;
	}
	return NULL;
}

static void print_device(GDBusProxy *proxy, const char *description){
	DBusMessageIter iter;
	const char *address, *name;

	if (g_dbus_proxy_get_property(proxy, "Address", &iter) == FALSE)
		return;

	dbus_message_iter_get_basic(&iter, &address);

	if (g_dbus_proxy_get_property(proxy, "Alias", &iter) == TRUE)
		dbus_message_iter_get_basic(&iter, &name);
	else
		name = "<unknown>";

	printf("%s%s%sDevice %s %s\n",
				description ? "[" : "",
				description ? : "",
				description ? "] " : "",
				address, name);
    device_added_callback(name, address);
	//TODO: Should filter using Service UUID
	// if(clear_devices){		
	// 	remove_all_devices();
	// 	clear_devices = FALSE;
	// 	// quit();
	// }

			
}

static void device_added(GDBusProxy *proxy){
	DBusMessageIter iter;
	struct adapter *adapter = find_parent(proxy);

	if (!adapter) {
		/* TODO: Error */
		return;
	}

	adapter->devices = g_list_append(adapter->devices, proxy);
	print_device(proxy, COLORED_NEW);

	if (default_dev)
		return;

	if (g_dbus_proxy_get_property(proxy, "Connected", &iter)) {
		dbus_bool_t connected;

		dbus_message_iter_get_basic(&iter, &connected);

		if (connected)
			printf("%d\n",connected);
			set_default_device(proxy, NULL);
	}
	// mainloop_quit();
}

static void device_removed(GDBusProxy *proxy){
	struct adapter *adapter = find_parent(proxy);
	if (!adapter) {
		/* TODO: Error */
		return;
	}
	adapter->devices = g_list_remove(adapter->devices, proxy);

	print_device(proxy, COLORED_DEL);	

	if (default_dev == proxy)
		set_default_device(NULL, NULL);
}

/* bluez GATT msg helper */
static gboolean service_is_child(GDBusProxy *service){
	DBusMessageIter iter;
	const char *device;

	if (g_dbus_proxy_get_property(service, "Device", &iter) == FALSE)
		return FALSE;

	dbus_message_iter_get_basic(&iter, &device);

	if (!default_ctrl)
		return FALSE;

	return g_dbus_proxy_lookup(default_ctrl->devices, NULL, device,
					"org.bluez.Device1") != NULL;
}

static void set_default_attribute(GDBusProxy *proxy){
	const char *path;

	default_attr = proxy;

	path = g_dbus_proxy_get_path(proxy);

	set_default_device(default_dev, path);
}

gboolean attribute_select(void){
    GDBusProxy *proxy;

	if (!default_dev) {
		printf("No device connected\n");
		return FALSE;
	}

	temp_attr = gatt_select_attribute(NULL, CURA8_TEMP_CHARACTERISTIC_UUID);
	cmd_attr = gatt_select_attribute(NULL, CURA8_CMD_CHARACTERISTIC_UUID);
	if (temp_attr && cmd_attr){
		return TRUE;
	}
		
	// proxy = gatt_select_attribute(NULL, CURA8_TEMP_CHARACTERISTIC_UUID);
	// if (proxy) {
	// 	set_default_attribute(proxy);

	// 	cmd_attr = gatt_select_attribute(NULL, CURA8_CMD_CHARACTERISTIC_UUID);
	// 	if (cmd_attr) {			
	// 		return TRUE;
	// 	}
	// }	

	return FALSE;
}

/* bluez Advertising Manager msg helper */
static void ad_manager_added(GDBusProxy *proxy){
	struct adapter *adapter;
	
	adapter = find_ctrl(ctrl_list, g_dbus_proxy_get_path(proxy));
	if (!adapter)
		adapter = adapter_new(proxy);

	adapter->ad_proxy = proxy;
	printf("ad_manager_added\n");
}



/* Client handlers */
 
static void proxy_leak(gpointer data){
	printf("Leaking proxy %p\n", data);
}

static void connect_handler(DBusConnection *connection, void *user_data){
	printf(PROMPT_ON);	
}

static void disconnect_handler(DBusConnection *connection, void *user_data){
	printf("disconnect_handler\n");

	g_list_free_full(ctrl_list, proxy_leak);

	default_ctrl = NULL;
}

static void message_handler(DBusConnection *connection,	DBusMessage *message, void *user_data){
	printf("[SIGNAL] %s.%s\n", dbus_message_get_interface(message),	dbus_message_get_member(message));
}

static void proxy_added_handler(GDBusProxy *proxy, void *user_data){
    const char *interface;

	interface = g_dbus_proxy_get_interface(proxy);

	if (!strcmp(interface, "org.bluez.Device1")) {
		device_added(proxy);
	} else if (!strcmp(interface, "org.bluez.Adapter1")) {
		adapter_added(proxy);
	}
	// } else if (!strcmp(interface, "org.bluez.AgentManager1")) {
	// 	if (!agent_manager) {
	// 		agent_manager = proxy;

	// 		if (auto_register_agent)
	// 			// agent_register(dbus_conn, agent_manager,
	// 						auto_register_agent);
	// 	}
	else if (!strcmp(interface, "org.bluez.GattService1")) {
		 if (service_is_child(proxy)){
			 //printf("interface added: %s\n", interface);
			 gatt_add_service(proxy);
		 }			
	} else if (!strcmp(interface, "org.bluez.GattCharacteristic1")) {
		//printf("interface added: %s\n", interface);
		//printf("[CHAR]");
		// printf("reached!\n");
		gatt_add_characteristic(proxy);
	} else if (!strcmp(interface, "org.bluez.GattDescriptor1")) {
		//printf("interface added: %s\n", interface);
		gatt_add_descriptor(proxy);
	} else if (!strcmp(interface, "org.bluez.GattManager1")) {
		printf("interface added: %s\n", interface);
		// gatt_add_manager(proxy);
	} else if (!strcmp(interface, "org.bluez.LEAdvertisingManager1")) {
		printf("interface added: %s\n", interface);
		ad_manager_added(proxy);
	}
}

static void proxy_removed_handler(GDBusProxy *proxy, void *user_data){
    const char *interface;

	interface = g_dbus_proxy_get_interface(proxy);	

	if (!strcmp(interface, "org.bluez.Device1")) {
		device_removed(proxy);
	} else if (!strcmp(interface, "org.bluez.Adapter1")) {
		printf("interface removed: %s\n", interface);
		// adapter_removed(proxy);
	} else if (!strcmp(interface, "org.bluez.AgentManager1")) {
		if (agent_manager == proxy) {
			agent_manager = NULL;
			if (auto_register_agent){
				// agent_unregister(dbus_conn, NULL);
			}
		}
	} else if (!strcmp(interface, "org.bluez.GattService1")) {
		gatt_remove_service(proxy);
		// printf("interface removed: %s\n", interface);
		if (default_attr == proxy){
			set_default_attribute(NULL);
		}
	} else if (!strcmp(interface, "org.bluez.GattCharacteristic1")) {
		// printf("interface removed: %s\n", interface);
		gatt_remove_characteristic(proxy);

		if (default_attr == proxy){
			set_default_attribute(NULL);
		}
	} else if (!strcmp(interface, "org.bluez.GattDescriptor1")) {
		// printf("interface removed: %s\n", interface);
		gatt_remove_descriptor(proxy);

		if (default_attr == proxy){
			set_default_attribute(NULL);
		}
	} else if (!strcmp(interface, "org.bluez.GattManager1")) {
		printf("interface removed: %s\n", interface);
		// gatt_remove_manager(proxy);
	} else if (!strcmp(interface, "org.bluez.LEAdvertisingManager1")) {
		printf("interface removed: %s\n", interface);
		// ad_unregister(dbus_conn, NULL);
	}
}

static void property_changed_handler(GDBusProxy *proxy, const char *name, DBusMessageIter *iter, void *user_data){
    const char *interface;
	struct adapter *ctrl;

	interface = g_dbus_proxy_get_interface(proxy);

	// printf("interface changed: %s\n", interface);

	if (!strcmp(interface, "org.bluez.Device1")) {
		if (default_ctrl && device_is_child(proxy,
					default_ctrl->proxy) == TRUE) {
			DBusMessageIter addr_iter;
			char *str;

			if (g_dbus_proxy_get_property(proxy, "Address",
							&addr_iter) == TRUE) {
				const char *address;

				dbus_message_iter_get_basic(&addr_iter,
								&address);
				str = g_strdup_printf("[" COLORED_CHG
						"] Device %s ", address);
			} else
				str = g_strdup("");

			if (strcmp(name, "Connected") == 0) {
				dbus_bool_t connected;

				dbus_message_iter_get_basic(iter, &connected);

				if (connected && default_dev == NULL)
					set_default_device(proxy, NULL);
				else if (!connected && default_dev == proxy)
					set_default_device(NULL, NULL);
			}

			print_iter(str, name, iter);

			if(strcmp(name, "ServicesResolved") == 0){
				dbus_bool_t Resolved;
				dbus_message_iter_get_basic(iter, &Resolved);

				if(Resolved){
					printf("ServicesResolved!\n");
					if(attribute_select()){
						printf("selected cura8 attr!\n");
						connection_successful_callback();
					}
					else 
						printf("Can not select cura8 attr!\n");
				}
			}
			g_free(str);
		}
	} 
	// else if (!strcmp(interface, "org.bluez.Adapter1")) {
	// 	DBusMessageIter addr_iter;
	// 	char *str;

	// 	if (g_dbus_proxy_get_property(proxy, "Address",
	// 					&addr_iter) == TRUE) {
	// 		const char *address;

	// 		dbus_message_iter_get_basic(&addr_iter, &address);
	// 		str = g_strdup_printf("[" COLORED_CHG
	// 					"] Controller %s ", address);
	// 	} else
	// 		str = g_strdup("");

	// 	print_iter(str, name, iter);
	// 	g_free(str);
	// } else if (!strcmp(interface, "org.bluez.LEAdvertisingManager1")) {
	// 	DBusMessageIter addr_iter;
	// 	char *str;

	// 	ctrl = find_ctrl(ctrl_list, g_dbus_proxy_get_path(proxy));
	// 	if (!ctrl)
	// 		return;

	// 	if (g_dbus_proxy_get_property(ctrl->proxy, "Address",
	// 					&addr_iter) == TRUE) {
	// 		const char *address;

	// 		dbus_message_iter_get_basic(&addr_iter, &address);
	// 		str = g_strdup_printf("[" COLORED_CHG
	// 					"] Controller %s ",
	// 					address);
	// 	} else
	// 		str = g_strdup("");

	// 	print_iter(str, name, iter);
	// 	g_free(str);
	// } 
	else if (proxy == default_attr || proxy == temp_attr || proxy == cmd_attr) {
		char *str;

		str = g_strdup_printf("[" COLORED_CHG "] Attribute %s ",
						g_dbus_proxy_get_path(proxy));

		print_iter(str, name, iter);

		g_free(str);
	}
}

/* Client control commands  */
static void start_discovery_reply(DBusMessage *message, void *user_data){
	dbus_bool_t enable = GPOINTER_TO_UINT(user_data);
	DBusError error;

	dbus_error_init(&error);

	if (dbus_set_error_from_message(&error, message) == TRUE) {
		printf("Failed to %s discovery: %s\n",
				enable == TRUE ? "start" : "stop", error.name);
		dbus_error_free(&error);
		return ;
	}

	printf("Discovery %s\n", enable ? "started" : "stopped");
	scanning = enable;
	/* Leave the discovery running even on noninteractive mode */
}

#define	DISTANCE_VAL_INVALID	0x7FFF

static struct set_discovery_filter_args {
	char *transport;
	dbus_uint16_t rssi;
	dbus_int16_t pathloss;
	char **uuids;
	size_t uuids_len;
	dbus_bool_t duplicate;
	bool set;
} filter = {
	.rssi = DISTANCE_VAL_INVALID,
	.pathloss = DISTANCE_VAL_INVALID,
	.set = true,
};

static void set_discovery_filter_setup(DBusMessageIter *iter, void *user_data){
	struct set_discovery_filter_args *args = user_data;
	DBusMessageIter dict;

	dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY,
				DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
				DBUS_TYPE_STRING_AS_STRING
				DBUS_TYPE_VARIANT_AS_STRING
				DBUS_DICT_ENTRY_END_CHAR_AS_STRING, &dict);

	g_dbus_dict_append_array(&dict, "UUIDs", DBUS_TYPE_STRING,
							&args->uuids,
							args->uuids_len);

	if (args->pathloss != DISTANCE_VAL_INVALID)
		g_dbus_dict_append_entry(&dict, "Pathloss", DBUS_TYPE_UINT16,
						&args->pathloss);

	if (args->rssi != DISTANCE_VAL_INVALID)
		g_dbus_dict_append_entry(&dict, "RSSI", DBUS_TYPE_INT16,
						&args->rssi);

	if (args->transport != NULL)
		g_dbus_dict_append_entry(&dict, "Transport", DBUS_TYPE_STRING,
						&args->transport);

	if (args->duplicate)
		g_dbus_dict_append_entry(&dict, "DuplicateData",
						DBUS_TYPE_BOOLEAN,
						&args->duplicate);

	dbus_message_iter_close_container(iter, &dict);
}

static void set_discovery_filter_reply(DBusMessage *message, void *user_data){
	DBusError error;

	dbus_error_init(&error);
	if (dbus_set_error_from_message(&error, message) == TRUE) {
		printf("SetDiscoveryFilter failed: %s\n", error.name);
		dbus_error_free(&error);
		return ;
	}

	filter.set = true;

	printf("SetDiscoveryFilter success\n");

	return ;
}

static void set_discovery_filter(void){
	if (!default_ctrl)
		return;

	if (g_dbus_proxy_method_call(default_ctrl->proxy, "SetDiscoveryFilter",
		set_discovery_filter_setup, set_discovery_filter_reply,
		&filter, NULL) == FALSE) {
		printf("Failed to set discovery filter\n");
		return;
	}
	
	filter.set = true;
}

void scan(bool enable){
	const char *method;

	if (!default_ctrl)
		return;

	if (enable == TRUE) {
		set_discovery_filter();
		method = "StartDiscovery";
	} else
		method = "StopDiscovery";

	if (g_dbus_proxy_method_call(default_ctrl->proxy, method,
				NULL, start_discovery_reply,
				GUINT_TO_POINTER(enable), NULL) == FALSE) {
		printf("Failed to %s discovery\n",
					enable == TRUE ? "start" : "stop");
	}
}

static GDBusProxy *find_proxy_by_address(GList *source, const char *address){
	GList *list;

	for (list = g_list_first(source); list; list = g_list_next(list)) {
		GDBusProxy *proxy = list->data;
		DBusMessageIter iter;
		const char *str;

		if (g_dbus_proxy_get_property(proxy, "Address", &iter) == FALSE)
			continue;

		dbus_message_iter_get_basic(&iter, &str);

		if (!strcasecmp(str, address))
			return proxy;
	}

	return NULL;
}

static void connect_reply(DBusMessage *message, void *user_data){
	GDBusProxy *proxy = user_data;
	DBusError error;

	dbus_error_init(&error);

	if (dbus_set_error_from_message(&error, message) == TRUE) {
		printf("Failed to connect: %s\n", error.name);
		dbus_error_free(&error);
		return;
	}

	printf("Connection successful\n");

	set_default_device(proxy, NULL);
	
	return;
}

static void connect(const char *address){
	GDBusProxy *proxy;

	if (!default_ctrl)
		return;

	proxy = find_proxy_by_address(default_ctrl->devices, address);
	if (!proxy) {
		printf("Device %s not available\n", address);
		return;
	}

	if (g_dbus_proxy_method_call(proxy, "Connect", NULL, connect_reply,
							proxy, NULL) == FALSE) {
		printf("Failed to connect\n");
		return;
	}

	printf("Attempting to connect to %s\n", address);
}

static struct GDBusProxy * find_device(const char *address){
	GDBusProxy *proxy;

	if (!strlen(address)) {
		if (default_dev)
			return default_dev;
		printf("Missing device address argument\n");
		return NULL;
	}

	if (!default_ctrl)
		return NULL;

	proxy = find_proxy_by_address(default_ctrl->devices, address);
	if (!proxy) {
		printf("Device %s not available\n", address);
		return NULL;
	}

	return proxy;
}

static void print_property(GDBusProxy *proxy, const char *name){
	DBusMessageIter iter;

	if (g_dbus_proxy_get_property(proxy, name, &iter) == FALSE)
		return;

	print_iter("\t", name, &iter);
}

static void print_uuids(GDBusProxy *proxy){
	DBusMessageIter iter, value;

	if (g_dbus_proxy_get_property(proxy, "UUIDs", &iter) == FALSE)
		return;

	dbus_message_iter_recurse(&iter, &value);

	while (dbus_message_iter_get_arg_type(&value) == DBUS_TYPE_STRING) {
		const char *uuid;

		dbus_message_iter_get_basic(&value, &uuid);

		printf("\tUUID:\t\t%s \n",uuid);

		dbus_message_iter_next(&value);
	}
}

APP_RET_CODE print_info(const char *address){
	GDBusProxy *proxy;
	DBusMessageIter iter;

	proxy = find_device(address);
	if (!proxy)
		return APP_DEV_NOT_AVAILABLE;

	if (g_dbus_proxy_get_property(proxy, "Address", &iter) == FALSE)
		return APP_UNKOWN;

	dbus_message_iter_get_basic(&iter, &address);

	if (g_dbus_proxy_get_property(proxy, "AddressType", &iter) == TRUE) {
		const char *type;

		dbus_message_iter_get_basic(&iter, &type);

		// printf(COLOR_BLUE "[Device Info]" COLOR_OFF "%s (%s)\n", address, type);
		printf(COLOR_BLUE "\t[Device Info]" COLOR_OFF "\n");
	} else {
		printf("Device %s\n", address);
	}
	
	print_property(proxy, "Name");
	print_property(proxy, "Address");
	print_property(proxy, "Connected");
	print_uuids(proxy);

	return APP_SUCCESS;
}

void cura8_attempting_to_connect(const char *address){
	GDBusProxy *proxy;
	DBusMessageIter iter, value;
	bool correct_uuid = FALSE;

	proxy = find_device(address);
	if (!proxy)
		return;
	if (g_dbus_proxy_get_property(proxy, "Address", &iter) == FALSE)
		return;

	//check base uuid
	if (g_dbus_proxy_get_property(proxy, "UUIDs", &iter) == FALSE)
		return;

	dbus_message_iter_recurse(&iter, &value);

	while (dbus_message_iter_get_arg_type(&value) == DBUS_TYPE_STRING) {
		const char *uuid;

		dbus_message_iter_get_basic(&value, &uuid);

		if(!strcmp(uuid,CURA8_BASE_UUID))
			correct_uuid = TRUE;

		dbus_message_iter_next(&value);
	}

	//connect to
	if(correct_uuid)
		connect(address);
	else
		printf("Not found correct base UUID!\n");

	// quit();
}



void cura8_temperature_notify(dbus_bool_t enable){
    if (!temp_attr) {
		printf("No temperature attribute selected\n");
		return;
	}

	gatt_notify_attribute(temp_attr, enable ? true : false);
}

static void remove_device_reply(DBusMessage *message, void *user_data){
	DBusError error;

	dbus_error_init(&error);

	if (dbus_set_error_from_message(&error, message) == TRUE) {
		printf("Failed to remove device: %s\n", error.name);
		dbus_error_free(&error);
		return;
	}

	//printf("Device has been removed\n");
	return;
}

static void remove_device_setup(DBusMessageIter *iter, void *user_data){
	const char *path = user_data;

	dbus_message_iter_append_basic(iter, DBUS_TYPE_OBJECT_PATH, &path);
}

static void remove_device_proxy(GDBusProxy *proxy){
	char *path;

	path = g_strdup(g_dbus_proxy_get_path(proxy));

	if (!default_ctrl)
		return;

	if (g_dbus_proxy_method_call(default_ctrl->proxy, "RemoveDevice",
						remove_device_setup,
						remove_device_reply,
						path, g_free) == FALSE) {
		printf("Failed to remove device\n");
		g_free(path);
		return;
	}
}

void remove_all_devices(void){
	GList *list;

	for (list = default_ctrl->devices; list;
					list = g_list_next(list)) {
		GDBusProxy *proxy = list->data;

		remove_device_proxy(proxy);
	}
	default_dev = NULL;
	default_attr = NULL;
	// gatt_reset();
}

void remove_device(const char *address){
	GDBusProxy *proxy;

	if (!default_ctrl)
		return;

	proxy = find_proxy_by_address(default_ctrl->devices, address);
	if (!proxy) {
		printf("Device %s not available\n", address);
		return;
	}

	remove_device_proxy(proxy);
}

void cura8_command_write(CURA8_CMD_TYPE_t type, char *data, size_t len){
	uint8_t buffer[MAX_ATTR_VAL_LEN];

	buffer[0] = type;
	memcpy(buffer+1, data, len);

	if (!cmd_attr) {
		printf("No attribute selected\n");
		return;// bt_shell_noninteractive_quit(EXIT_FAILURE);
	}

	gatt_write_attribute(cmd_attr, 0, buffer, len + 1);
}

void __attribute__((weak)) device_added_callback(const char *name, const char *address){
}

void __attribute__((weak)) connection_successful_callback(void){
}

void __attribute__((weak)) cura8_received_data_callback(unsigned char *byte, int *len){

}