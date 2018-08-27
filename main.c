 /*
 * main.c
 *
 *  Created on: Jul 28, 2018
 *      Author: Liem Le
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <wordexp.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "bluetooth-shell.h"

#include "data_logging_handling.h"
#include "csv_logging.h"

#define DATA_UPDATE_INTERVAL	5

uint32_t step = 0;
const char *cura8_device_address;
time_t time_begin;
pthread_t thread_id;

static inline void quit(void){
	printf("\nQuit after 1 seconds...\n\n");
	usleep(1000 * 1000);
	mainloop_quit();
}

static void *data_logging_thread(void *data){
	while (1)
	{
		usleep(200000);
		// printf(" DataLoggindHandling \n");
		dataloggindhandling_main();
	}
	return NULL;
}

static gboolean cura8_reader_control_process(gpointer user_data){	
	if(default_ctrl){
		switch(step){
			case 0:{
				printf("check_device_list_process done\n");
				if(clear_devices)remove_all_devices();
				++step;
				return TRUE;
				break;
			}
			case 1:{
				scan(TRUE);
				++step;
				return TRUE;
				break;
			}
			case 3:{		
				scan(FALSE);
				++step;
				return TRUE;
				break;
			}
			case 4:{
				if(print_info(cura8_device_address) == APP_SUCCESS){
					cura8_attempting_to_connect(cura8_device_address);
					++step;	
					return FALSE;
				}
				else{
					step = 1;
					return TRUE;
				}			
				break;
			}
			case 6:{ // Send security key
				cura8_command_write(CURA8_SEC, CURA8_READER_SEC_KEY, sizeof(CURA8_READER_SEC_KEY));
				++step;
				return TRUE;
				break;
			}
			case 7:{ // Send update interval
				char val;
				val = DATA_UPDATE_INTERVAL;
				if(write_done){
					cura8_command_write(CURA8_UPDATE_INTERVAL, &val, sizeof(val));
					cura8_temperature_notify(TRUE);
					time_begin = time(NULL);
					++step;
				}
				return TRUE;
				break;
			}
			case 8:{
				++step;
				return FALSE;
				break;
			}
			case 10:{
				cura8_temperature_notify(FALSE);
				++step;
				return FALSE;
				break;
			}
			case 12:{
				// if(write_done){
					remove_all_devices();
					++step;
				// }
				return TRUE;
				break;
			}
			case 13:{
				quit();
				++step;
				return TRUE;
				break;
			}
			default :{
				return TRUE;
				break;
			}
		}
	}
}

static gboolean cura8_reader_idle_process(gpointer user_data){
	if(step==9){
		printf("changed task timeout!\n");
		g_timeout_add(20000, cura8_reader_control_process, NULL);
		step = 10;
	}
	else if (step == 11){
		g_timeout_add(500, cura8_reader_control_process, NULL);
		step = 12;
	}
	return TRUE;
}

int main(int argc, char *argv[]){
	GDBusClient *client;
	int status;

	mainloop_init();
	
	dataloggindhandling_init();
	csv_logging_init();

	bluetooth_shell_init();

	g_timeout_add(500, cura8_reader_control_process, NULL);
	g_idle_add(cura8_reader_idle_process,NULL);

	printf("Main loop!\n");	
	status = mainloop_run();
	printf("Exit main loop status: %d\n", status);

	bluetooth_shell_destruct();
	return status;
}

void connection_successful_callback(void){
	pthread_create(&thread_id, NULL, data_logging_thread, NULL);
	attempting_quit = true;
	step = 6;
	g_timeout_add(200, cura8_reader_control_process, NULL);
}

void device_added_callback(const char *name, const char *address){
	if(scanning){
		if(!strcmp(name, CURA8_SENSOR_NAME)){
			printf("found cura8!\n");
			cura8_device_address = address;
			step = 3;
		}
	}
	else{
		clear_devices = TRUE;
	}
}

static inline void print_temp_char(const char *src){
	struct{
		float 		temp;
		uint32_t 	timestamp;
	}tmp;
	memcpy(&tmp,src,8);
	time_t tempt= time_begin + tmp.timestamp; ;

	struct tm *tm = localtime(&tempt);
	char s[64];
    strftime(s, sizeof(s), "%c", tm);
	printf(COLOR_BLUE "[Temperature: %2.2f]" COLOR_YELLOW "\t[Timestamp: %d]" COLOR_GREEN "\t[%s]\n" COLOR_OFF,tmp.temp,tmp.timestamp, s);
}

static inline void push_data_to_queue(const char * data){
	struct{
		float 		temp;
		uint32_t 	timestamp;
	}tmp;

	ble_data_t record;
	memset(&record, 0, sizeof(record));
	time_t tempt= time_begin + tmp.timestamp; ;
	struct tm *tm = localtime(&tempt);
	char s[64];
    strftime(s, sizeof(s), "%c", tm);
	memcpy(record.ID, cura8_device_address, sizeof(record.ID));
	record.temperature_f = tmp.temp;
	memcpy(record.time, s, sizeof(s));
	ble_queue_push_data(record);
}

void cura8_received_data_callback(unsigned char *byte, int *len){
	size_t item_len = *len / 8;
	printf("item_received_len: %d\n", item_len);
	for(size_t i = 0; i < item_len; i++){
		print_temp_char(byte + i*8);
		push_data_to_queue(byte +i*8);
	}
}
