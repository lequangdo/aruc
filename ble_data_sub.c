/*
 * BLEDataLog.c
 *
 *  Created on: Aug 2, 2018
 *      Author: dole
 */


#include <stdio.h>
#include <string.h>
#include "ble_data_sub.h"

//#include "data_logging_handling.h"

bledatasub_t* bledatapool = NULL;

static void sub_destroy(bledatasub_t* this)
{
	if (this != NULL) {
		free(this);
		this = NULL;
	}
}

static int sub_addlistener(obj_t* observer)
{
	int i = 0;
	bledatasub_t* this = bledatapool;
	for (; i < MAX_OBSERVERS; i++) {
		if (this->observers[i] == NULL) {
			this->observers[i] = observer;
			return EXIT_SUCCESS;
		}
	}

	return EXIT_FAILURE;
}
static int sub_rmvlistener(obj_t * observer)
{
	int i = 0;
	bledatasub_t * this =  bledatapool;
	for (; i < MAX_OBSERVERS; i++) {
		void* pObserver = this->observers[i];

		if (observer == pObserver) {
			pObserver = NULL;
			return EXIT_SUCCESS;
		}
	}

	return EXIT_FAILURE;
}
static void sub_datarx_noti(void)
{
	bledatasub_t * this = bledatapool;
	/* Call all the notification function of all observers */

	for (int i = 0; i < MAX_OBSERVERS; i++) {
		if (this->observers[i] != NULL) {
			this->observers[i]->noti_fptr();
		}
	}
}

bledatasub_t * subcreate(void)
{
	bledatasub_t* this = (bledatasub_t *) malloc(sizeof(*this));

	this->destroy = 		sub_destroy;
	this->sub_addlistener = sub_addlistener;
	this->sub_rmvlistener = sub_rmvlistener;
	this->sub_datarx_noti = (void (*)(void))sub_datarx_noti;

	return this;
}
