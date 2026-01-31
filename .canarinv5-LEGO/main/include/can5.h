#include <sys/cdefs.h>
/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   11/01/22
*
* File:  init.h
* Descr:
*******************************************************************************/

#ifndef CAN5_APP_CAN5_H
#define CAN5_APP_CAN5_H

#include "can5_error.h"
#include "can5_types.h"


can5_err_t can5_modules_init(bool *ota_upgrade);

can5_err_t can5_init();

can5_err_t can5_ota_init();

can5_err_t can5_ota_upgrade();

void can5_run(bool ota_upgrade);

can5_err_t can5_tests();

#endif //CAN5_APP_CAN5_H
