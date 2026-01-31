/**************************************************
 * Author: rmukhia
 * Creation Date: 29/8/22
 * Description: 
 **************************************************/
#include "can5_cron.h"
#include "esp_log.h"
#include "can5_pins.h"
#include "can5_cmdr.h"
#include <time.h>
#include <can5_utils.h>
#include <malloc.h>
#include <ccronexpr.h>
#include <sys/stat.h>

const static char *TAG = "CRON";

#if 0
#define TRACE_FUNC ESP_LOGI(TAG, "in -> %s() :%d", __FUNCTION__, __LINE__)
#else
#define TRACE_FUNC
#endif

#ifdef CAN5_LINUX_HOST_TEST
#define CRONTAB_FILE    "data/config/crontab"
#else
#define CRONTAB_FILE    CAN5_FATFS_MOUNT_DIR "/config/crontab"
#endif

#define DEF_CRON_JOBS(_token, _cb)          { .token = _token, .cb = _cb }

#define CRON_FUNCTION(_name, _now)               void _name(time_t _now)

typedef struct cron_jobs_def_s{
    const char *token;
    void (*cb)(time_t now);
} cron_jobs_def_t;



static char *default_crontab =
    "50 59 23 * * *  pause_lwan\n"                  \
    "0 0 0 * * *    reset_lwan_frame_counters\n"    \
    "0 1 0 * * *    resume_lwan\n";

static CRON_FUNCTION(__job_stub, now);

static CRON_FUNCTION(__job_restart, now);
static CRON_FUNCTION(__job_clear_all_sensor_cache, now);
static CRON_FUNCTION(__job_clear_old_sensor_cache, now);

static CRON_FUNCTION(__job_pause_lwan, now);
static CRON_FUNCTION(__job_reset_lwan_frame_counter, now);
static CRON_FUNCTION(__job_resume_lwan, now);

static const cron_jobs_def_t cron_jobs_def[] = {
    DEF_CRON_JOBS("stub", __job_stub),

    DEF_CRON_JOBS("restart", __job_restart),
    DEF_CRON_JOBS("clear_all_sensor_cache", __job_clear_all_sensor_cache),
    DEF_CRON_JOBS("clear_old_sensor_cache", __job_clear_old_sensor_cache),

    DEF_CRON_JOBS("pause_lwan", __job_pause_lwan),
    DEF_CRON_JOBS("reset_lwan_frame_counters", __job_reset_lwan_frame_counter),
    DEF_CRON_JOBS("resume_lwan", __job_resume_lwan),
};

typedef struct cron_jobs_s {
    cron_expr expr;
    void (*cb)(time_t now);
    const char *token;
} cron_jobs_ctx_t;

static struct cron_ctx_s {
    cron_jobs_ctx_t *jobs;
    size_t len;
} cron_ctx = {
    .jobs = NULL,
    .len = 0,
};

const char *can5_get_default_crontab()
{
    TRACE_FUNC;

    return default_crontab;
}

can5_err_t can5_write_crontab(const char *crontab)
{
    TRACE_FUNC;

    FILE *file;
    size_t len;
    can5_err_t res;

    file = NULL;
    res = CAN5_SUCCESS;

    file = fopen(CRONTAB_FILE, "w");

    if (!file) {
        res = CAN5_STORAGE_ERR_FILESYSTEM;
        goto done;
    }

    len = strlen(crontab);

    if (fwrite(crontab, len, 1, file) != 1) {
        res = CAN5_STORAGE_ERR_FILESYSTEM;
        goto done;
    }

done:
    if (file) {
        fclose(file);
    }
    return res;
}

can5_err_t can5_cron_get_jobs(const char **jobs, size_t *len)
{
    TRACE_FUNC;

    *len = 0;

    for (size_t i = 0; i < sizeof(cron_jobs_def) / sizeof(cron_jobs_def[0]); i++) {
        jobs[i] = cron_jobs_def[i].token;
        *len = *len + 1;
    }

    return CAN5_SUCCESS;
}

can5_err_t can5_read_crontab(char **crontab)
{
    TRACE_FUNC;

    FILE *file;
    can5_err_t res;
    struct stat st;

    file = NULL;
    res = CAN5_SUCCESS;

    if (stat(CRONTAB_FILE, &st)) {
        return CAN5_STORAGE_ERR_FILESYSTEM;
    }

    VERIFY_ALLOC(*crontab, st.st_size + 1);

    if (!*crontab) {
        res = CAN5_ERR_OUT_OF_HEAP_MEMORY;
        goto done;
    }

    file = fopen(CRONTAB_FILE, "r");

    if (!file) {
        res = CAN5_STORAGE_ERR_FILESYSTEM;
        goto done;
    }

    if (fread(*crontab, st.st_size, 1, file) != 1) {
        res = CAN5_STORAGE_ERR_FILESYSTEM;
        goto done;
    }

done:

    if (file) {
        fclose(file);
    }
    return res;
}

can5_err_t can5_cron_init()
{
    TRACE_FUNC;

    char *str, *r_ptr, *token;
    char *crontab;
    if (can5_read_crontab(&crontab) != CAN5_SUCCESS) {
        VERIFY_SUCCESS(can5_write_crontab(can5_get_default_crontab()));
        crontab = (char *)can5_get_default_crontab();
    }

    str = r_ptr = strdup(crontab);
    token = strtok_r(str, "\n", &r_ptr);

    while (token) {
        char *cronexpr, *cb_func, *partition;

        partition = strrchr(token, ' ');
        if (!partition) {
            goto next;
        }

        cronexpr = token;
        cb_func = partition + 1;
        remove_spaces(cb_func);
        *partition = '\0';

        ESP_LOGI(TAG, "Cron Jobs:");
        ESP_LOGI(TAG, "%s, %s\n", cronexpr, cb_func);

        for (size_t i = 0; i < sizeof(cron_jobs_def)/ sizeof(cron_jobs_def[0]); i++) {
            const cron_jobs_def_t *def = &cron_jobs_def[i];
            cron_jobs_ctx_t *job_ctx;
            if (strncmp(def->token, cb_func, strlen(def->token)) == 0) {
                const char *err = NULL;
                cron_ctx.len++;
                cron_ctx.jobs = realloc(cron_ctx.jobs, sizeof(cron_jobs_ctx_t) * cron_ctx.len);
                job_ctx = &cron_ctx.jobs[cron_ctx.len - 1];
                job_ctx->token = def->token;
                job_ctx->cb = def->cb;
                cron_parse_expr(cronexpr, &job_ctx->expr, &err);
                if (err) {
                    ESP_LOGE(TAG, "%s", err);
                    cron_ctx.len--;
                }
            }
        }
next:
        token = strtok_r(NULL, "\n", &r_ptr);

    }

    free(str);


    return CAN5_SUCCESS;
}

can5_err_t can5_cron_next_time(time_t cur_time, time_t *next_time)
{
    TRACE_FUNC;

    if (!cron_ctx.len) {
        return CAN5_ERROR;
    }

    cron_jobs_ctx_t *job_ctx = &cron_ctx.jobs[0];
    // get the closest time next
    *next_time = cron_next(&job_ctx->expr, cur_time);
    for(size_t i = 1; i < cron_ctx.len; i++) {
        job_ctx = &cron_ctx.jobs[i];
        time_t j_time = cron_next(&job_ctx->expr, cur_time);
        if (j_time < *next_time) {
            *next_time = j_time;
        }
    }

    return CAN5_SUCCESS;
}

can5_err_t can5_cron_run_jobs(time_t cur_time)
{
    TRACE_FUNC;

    for(size_t i = 0; i < cron_ctx.len; i++) {
        cron_jobs_ctx_t *job_ctx = &cron_ctx.jobs[i];
        time_t j_time = cron_next(&job_ctx->expr, cur_time - 1);
        if (j_time == cur_time) {
            job_ctx->cb(cur_time);
        }
    }

    return CAN5_SUCCESS;
}

void can5_cron_free()
{
    TRACE_FUNC;

    free(cron_ctx.jobs);
}

static CRON_FUNCTION(__job_stub, now)
{
    TRACE_FUNC;

    ESP_LOGI(TAG, "STUB %s", asctime(gmtime(&now)));
}

static CRON_FUNCTION(__job_restart, now)
{
    TRACE_FUNC;

    can5_cmd_params_t params = {
        .restart_after = 0,
    };

    CAN5_ERR_CHECK(can5_commander.add_cmd(CAN5_CMD_RESET_AFTER, &params, NULL, NULL));
}

static CRON_FUNCTION(__job_clear_all_sensor_cache, now)
{
    TRACE_FUNC;

    CAN5_ERR_CHECK(can5_commander.add_cmd(CAN5_CMD_CLEAR_ALL_SENSOR_DATA, NULL, NULL, NULL));
}

static CRON_FUNCTION(__job_clear_old_sensor_cache, now)
{
    TRACE_FUNC;

    CAN5_ERR_CHECK(can5_commander.add_cmd(CAN5_CMD_CLEAR_OLD_SENSOR_DATA, NULL, NULL, NULL));
}

static CRON_FUNCTION(__job_pause_lwan, now)
{
    TRACE_FUNC;

    can5_cmd_params_t params = {
        .cmdr_event = {
            .event = CAN5_CMDR_EVT_LWAN_PAUSE,
            .timeout_ms = portMAX_DELAY,
        }
    };
    CAN5_ERR_CHECK(can5_commander.add_cmd(CAN5_CMD_POST_EVENT, &params, NULL, NULL));
}

static CRON_FUNCTION(__job_reset_lwan_frame_counter, now)
{
    TRACE_FUNC;

    can5_cmd_params_t params = {
        .cmdr_event = {
            .event = CAN5_CMDR_EVT_LWAN_RESET_FRAME_COUNT,
            .timeout_ms = portMAX_DELAY,
        }
    };
    CAN5_ERR_CHECK(can5_commander.add_cmd(CAN5_CMD_POST_EVENT, &params, NULL, NULL));

}

static CRON_FUNCTION(__job_resume_lwan, now)
{
    TRACE_FUNC;

    can5_cmd_params_t params = {
        .cmdr_event = {
            .event = CAN5_CMDR_EVT_LWAN_RESUME,
            .timeout_ms = portMAX_DELAY,
        }
    };
    CAN5_ERR_CHECK(can5_commander.add_cmd(CAN5_CMD_POST_EVENT, &params, NULL, NULL));
}
