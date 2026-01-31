/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   12/01/22
*
* File:  can5_rtc.h
* Descr:
*******************************************************************************/

#ifndef CAN5_APP_CAN5_RTC_H
#define CAN5_APP_CAN5_RTC_H
#include "can5_error.h"
#include "can5_module.h"
typedef enum can5_rtc_hook_type_e {
    CAN5_RTC_HOOKS_TIME_UPDATE = 0,
    CAN5_RTC_HOOKS_PRE_SLEEP,
    CAN5_RTC_HOOKS_POST_SLEEP,

    CAN5_RTC_HOOKS_COUNT,
} can5_rtc_hook_type_t;

typedef can5_err_t (*can5_rtc_hook_t) (void);

typedef struct can5_rtc_s {
    can5_module_t module;
    can5_err_t (*set_time)(const struct tm *tm);
    can5_err_t (*get_time)(struct tm *tm);
    can5_err_t (*rtc_to_sys)();
    can5_err_t (*sys_to_rtc)();
    can5_err_t (*set_alarm)(const struct tm *tm);
    can5_err_t (*sleep)();
    can5_err_t (*register_hook)(can5_rtc_hook_type_t hook_type, can5_rtc_hook_t hook_cb);
    int64_t (*get_current_uptime_usec)();
    can5_err_t (*adjust_gps_time)(struct tm *tm, int64_t acquired_usec);
    can5_err_t (*print_rtc_time)();
    can5_err_t (*reset_to_default_time)();
} can5_rtc_t;

#define MS_TO_SEC(x)    (x/1000)
#define MS_TO_USEC(x)   (x * 1000)
#define USEC_TO_MS(x)   (x/1000)

extern can5_rtc_t rtc;

#endif //CAN5_APP_CAN5_RTC_H
