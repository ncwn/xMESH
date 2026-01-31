/**************************************************
 * Author: rmukhia
 * Creation Date: 29/8/22
 * Description: 
 **************************************************/

#ifndef TEST_APP_CAN5_CRON_H
#define TEST_APP_CAN5_CRON_H

#include <time.h>
#include "can5_error.h"

typedef enum cron_jobs_type_e {
    CAN5_CRON_RESTART,
    CAN5_CRON_CLEAR_ALL_SENSOR_CACHE,
    CAN5_CRON_CLEAR_OLD_SENSOR_CACHE,
} cron_jobs_type_t;

typedef struct can5_cron_job_s {
    void (*cb)(time_t now);
    const char *token;
    time_t next;
} can5_cron_job_t;

can5_err_t can5_write_crontab(const char *crontab);

can5_err_t can5_read_crontab(char **crontab);

can5_err_t can5_cron_get_jobs(const char **jobs, size_t *len);

can5_err_t can5_cron_init();

const char *can5_get_default_crontab();

can5_err_t can5_cron_next_time(time_t cur_time, time_t *next_time);

can5_err_t can5_cron_run_jobs(time_t cur_time);

void can5_cron_free();

#endif //TEST_APP_CAN5_CRON_H
