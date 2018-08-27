/*
 * CSVLog.h
 *
 *  Created on: Aug 2, 2018
 *      Author: dole
 */

#ifndef BLEDATAOBJ_H_
#define BLEDATAOBJ_H_

#include <stdio.h>
#include <stdlib.h>

//#include "ble_data_sub.h"

typedef struct _object
{
	void (*noti_fptr)(void);
	void (*destroy_fptr)(struct _object*);
	void (*addlistener_fptr)(struct _object*);
} obj_t;

extern obj_t* objcreate(void (*)(void));

#endif /* BLEDATAOBJ_H_ */
