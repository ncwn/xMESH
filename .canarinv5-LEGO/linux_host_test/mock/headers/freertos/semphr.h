/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   03/02/22
*
* File:  semphr.h
* Descr:
*******************************************************************************/

#ifndef TEST_APP_SEMPHR_H
#define TEST_APP_SEMPHR_H

#include <semaphore.h>
#include <time.h>

typedef void * SemaphoreHandle_t;

#define vSemaphoreCreateBinary(x)
#define xSemaphoreGive(x)           true
#define xSemaphoreTake(x, timeout)  true
#define vSemaphoreDelete(x)
#define xSemaphoreCreateMutex()     NULL


#endif //TEST_APP_SEMPHR_H
