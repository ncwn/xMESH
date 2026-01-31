/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   25/02/22
*
* File:  can5_httpserver.h
* Descr:
*******************************************************************************/

#ifndef CAN5_APP_CAN5_HTTPSERVER_H
#define CAN5_APP_CAN5_HTTPSERVER_H
#include "can5_module.h"

typedef struct can5_httpserver_s {
    can5_module_t module;
} can5_httpserver_t;

extern can5_httpserver_t httpserver;

#endif //CAN5_APP_CAN5_HTTPSERVER_H
