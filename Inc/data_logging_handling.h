/*
 * DataReading.h
 *
 *  Created on: Aug 8, 2018
 *      Author: dole
 */

#ifndef DATALOGGINGHANDLING_H_
#define DATALOGGINGHANDLING_H_
#include "ble_data_obj.h"
#include "ble_data_sub.h"
#include <pthread.h>
#define MAX_QUEUE_MEM 1000

/*
typedef enum
{
	FALSE = 0,
	TRUE
}boolean_t;
*/

typedef struct
{
	char ID[18];			// Address ID
	float temperature_f;	// temperature xx.x
	char time[64];
}ble_data_t;

typedef struct
{
	unsigned int count;
	ble_data_t queue_data[MAX_QUEUE_MEM];
} queue_t;

/*************************************************/
extern pthread_mutex_t mutex1;
extern queue_t BLE_Data_Queue;

/* List of extern functions */
extern void ble_queue_push_data(ble_data_t ble_data);
extern void ble_queue_read_data(ble_data_t * buf);
extern void ble_queue_pop_data(unsigned int index, queue_t *q);

extern void dataloggindhandling_init();
extern void dataloggindhandling_main();

#endif /* DATALOGGINGHANDLING_H_ */
