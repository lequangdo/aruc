/*
 * DataReading.c
 *
 *  Created on: Aug 8, 2018
 *      Author: dole
 */

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "ble_data_sub.h"
#include "data_logging_handling.h"


pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
/* queue of ble data */
queue_t BLE_Data_Queue;


static void queue_init(queue_t* q);
static char is_queue_full(queue_t*q);
static char is_queue_empty(queue_t*q);
void ble_queue_pop_data(unsigned int index, queue_t *q);
void ble_queue_push_data(ble_data_t ble_data);
void ble_queue_read_data(ble_data_t * buf);

void dataloggindhandling_init()
{
	queue_init(&BLE_Data_Queue);
	bledatapool = subcreate();
}

void dataloggindhandling_main()
{
	queue_t * q = &BLE_Data_Queue;
	unsigned char temp = 0 ;

	temp = is_queue_empty(q);
	if(1 == temp){
		/* do not need to call the listener */
	}
	else{
		//printf(" Notify all listener that data is on the queue now\n");
		bledatapool->sub_datarx_noti();

		/* Pop the first data out of the queue after logging it to all listeners */
		pthread_mutex_lock( &mutex1 );
		ble_queue_pop_data(0, &BLE_Data_Queue);
		pthread_mutex_unlock( &mutex1 );
	}
}

static void queue_init(queue_t* q)
{
	memset(q, 0, sizeof(q));
	q->count = 0;
}

static char is_queue_full(queue_t*q)
{
	if(MAX_QUEUE_MEM == q->count)
		return 1;
	else
		return 0;
}

static char is_queue_empty(queue_t*q)
{
	if(0 == q->count)
		return 1;
	else
		return 0;
}
void ble_queue_pop_data(unsigned int index, queue_t *q)
{
	unsigned int i = index;
	if(1 == is_queue_empty(q))
	{
		/* Do nothing */
//		printf("queue empty\n");
	}
	else
	{
//		printf("pop data\n");
		for(; i< (q->count); i++)
		{
			q->queue_data[i] = q->queue_data[i+1];
		}
		q->count--;
	}
}
void ble_queue_push_data(ble_data_t ble_data)
{
	pthread_mutex_lock( &mutex1 );
	queue_t * q = &BLE_Data_Queue;
	if(1 == is_queue_full(q))
	{
		/* Do nothing */
//		printf("queue is full\n");
	}
	else
	{
//		printf("push data \n");
		memset(&q->queue_data[q->count], 0, sizeof(ble_data_t));
		memcpy(&q->queue_data[q->count], &ble_data, sizeof(ble_data_t));

		q->count++;
	}
	pthread_mutex_unlock( &mutex1 );
}

void ble_queue_read_data(ble_data_t * buf)
{
	queue_t * q = &BLE_Data_Queue;
	unsigned int i = 0;
	if(0 == is_queue_empty(q))
	{
		//printf("Read data \n");
		memcpy(buf, &q->queue_data[0], sizeof(ble_data_t));
	}
}

