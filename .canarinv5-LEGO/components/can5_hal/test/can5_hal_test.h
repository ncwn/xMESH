#ifndef __CAN5_HAL_TEST__
#define __CAN5_HAL_TEST__

#include "can5_error.h"

can5_err_t hal_test_serial();
can5_err_t hal_test_port_enable();
can5_err_t hal_test_port_reenable();
can5_err_t hal_test_serial_port_switch();
can5_err_t hal_test_i2c();
can5_err_t hal_test_adc();


#endif  // __CAN5_HAL_TEST__