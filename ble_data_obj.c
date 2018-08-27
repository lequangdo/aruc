/*
 * CSVLog.c
 *
 *  Created on: Aug 2, 2018
 *      Author: dole
 */
#include "ble_data_obj.h"
#include "ble_data_sub.h"
//#include "data_logging_handling.h"

static void obj_destroy(obj_t* this)
{
	if (this != NULL){
		free(this);
		this = NULL;
	}
}
static void obj_addlistener(obj_t* obj)
{
	//Let the blepool knows that we have another obj need to inform when rx data comes
	bledatapool->sub_addlistener(obj);
}

obj_t* objcreate( void (*fptr)(void))
{
	obj_t* this = (obj_t*) malloc(sizeof(*this));
	this->addlistener_fptr = obj_addlistener;
	this->destroy_fptr = obj_destroy;

	this->noti_fptr = (void (*)(void))fptr;

	return this;
}
