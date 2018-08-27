/*
 * DataLoggingHandling.c
 *
 *  Created on: Aug 2, 2018
 *      Author: dole
 */




#include <stdio.h>
#include <string.h>
#include "csv_logging.h"
#include "ble_data_obj.h"
#include "data_logging_handling.h"



void csv_datarx_noti(void){
	ble_data_t record;
	memset(&record, 0, sizeof(record));
	char data_s[248];
	memset(data_s,0,sizeof(data_s));
	FILE * pFile;
	// static char file_path[248];

	// sprintf(file_path, "/home/pi/sam-%s.csv", record.ID, record.temperature_f, record.time);
	ble_queue_read_data(&record);
	//if csv not exist, create it
	//else open and update it.
  	//pFile = fopen ("/home/dole/Documents/Code/Cura8/sam.csv","a");
	pFile = fopen ("/home/pi/sam.csv","a");
	if(NULL != pFile)
	{
		sprintf(data_s, "%s,%2.2f,%s \n", record.ID, record.temperature_f, record.time);
		fputs(data_s, pFile );
	}
	fclose(pFile);
}

void csv_logging_init()
{
	printf("create csv obj\n");
	obj_t * CSV = objcreate(csv_datarx_noti);
	printf("register CSV as a listener\n");
	CSV->addlistener_fptr(CSV);
}

void csv_logging_main()
{

}
