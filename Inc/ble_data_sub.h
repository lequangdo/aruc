/*
 * BLEDataLog.h
 *
 *  Created on: Aug 2, 2018
 *      Author: dole
 */

#ifndef BLEDATASUB_H_
#define BLEDATASUB_H_

#include "ble_data_obj.h"

#define MAX_OBSERVERS 10

typedef struct __bledatasub
{
	void (*destroy)(struct __bledatasub*);
	void (*sub_datarx_noti)(void);
	int (*sub_addlistener)(obj_t*);
	int (*sub_rmvlistener)(obj_t*);
	obj_t * observers[MAX_OBSERVERS];

} bledatasub_t;

extern bledatasub_t * subcreate(void);
extern bledatasub_t* bledatapool;
#endif /* BLEDATALOG_H_ */
