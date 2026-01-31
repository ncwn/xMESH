/**************************************************
 * Author: rmukhia
 * Creation Date: 28/6/22
 * Description: 
 **************************************************/
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <esp_log.h>
#include <malloc.h>
#include "can5_logstream_sd.h"
#include "can5_error.h"
#include "can5_storage_fsfat.h"
#include "can5_utils.h"

const static char *TAG = "LOG_STREAM_DISK";

#define LOG_FLUSH_INTERVAL      10              // 10 seconds
#define LOG_ROTATE_INTERVAL     60              // 1 minute

#define LOG_FILE                "LOG"
#define MAX_LOG_FILES           CONFIG_CAN5_STORAGE_LOG_NUM_MAX
#define EXPECTED_MAX_LOG_SIZE   (1048576 * CONFIG_CAN5_STORAGE_LOG_SIZE_MAX)


static can5_err_t init(int index, const can5_logger_activate_params_t *params, active_cb active_cb);

static can5_err_t uninit();

static void logstream_log(const can5_logger_msg_t *msg);

static can5_err_t logstream_flush();

static bool is_active();

can5_logstream_t can5_logstream_sd = {
    .type = CAN5_LOGGER_STREAM_SD,
    .init = init,
    .uninit = uninit,
    .flush = logstream_flush,
    .log = logstream_log,
    .is_active = is_active,
};

static struct {
    FILE *log_file;
    time_t log_last_sync;
    time_t log_last_rotate;
    bool is_active;
} __log_stream = {
    .log_file = NULL,
    .log_last_sync = 0,
    .log_last_rotate = 0,
    .is_active = false,
};

static can5_err_t __flush_sd_log(time_t now, bool check_interval);

static esp_err_t __log_open_file();


static can5_err_t init(int index, const can5_logger_activate_params_t *params, active_cb active_cb)
{
    ESP_LOGI(TAG, "Max log files %i of size %i.", CONFIG_CAN5_STORAGE_LOG_NUM_MAX, CONFIG_CAN5_STORAGE_LOG_SIZE_MAX);
    VERIFY_SUCCESS(__log_open_file());

    __log_stream.is_active = true;

    active_cb(index);

    return CAN5_SUCCESS;
}

static can5_err_t uninit()
{
    return CAN5_SUCCESS;
}

static can5_err_t logstream_flush()
{
    return __flush_sd_log(0, false);
}


static esp_err_t __log_open_file()
{
    __log_stream.log_file = fopen(CAN5_STORAGE_FSFAT_LOG_DIR LOG_FILE, "a");

    if (__log_stream.log_file == NULL) {
        return CAN5_STORAGE_ERR_FILESYSTEM;
    }

    return CAN5_SUCCESS;
}

static void __log_close_file()
{
    if (__log_stream.log_file) {
        fclose(__log_stream.log_file);
        __log_stream.log_file = NULL;
    }
}

static can5_err_t __flush_sd_log(time_t now, bool check_interval)
{
    if (check_interval && __log_stream.log_last_sync + LOG_FLUSH_INTERVAL < now) {
        return CAN5_SUCCESS;
    }

    if (!__log_stream.log_file) {
        return CAN5_STORAGE_ERR_FILE_NOT_FOUND;
    }

    fsync(fileno(__log_stream.log_file));
    __log_stream.log_last_sync = now;

    return CAN5_SUCCESS;
}


static bool __should_rotate_log(time_t now)
{
    struct stat sb;

    if (__log_stream.log_last_rotate + LOG_ROTATE_INTERVAL < now) {
        return false;
    }

    if (fstat(fileno(__log_stream.log_file), &sb) == -1) {
        return false;
    }

    if (sb.st_size < EXPECTED_MAX_LOG_SIZE) {
        return false;
    }

    __log_stream.log_last_rotate = now;

    return true;
}

static void __check_and_rename(int idx)
{
    char spath[32], dpath[32];
    struct stat sb;

    if (idx == 0) {
        strcpy(spath, CAN5_STORAGE_FSFAT_LOG_DIR LOG_FILE);
        strcpy(dpath, CAN5_STORAGE_FSFAT_LOG_DIR "LOG.1");
    } else {
        snprintf(spath, 32, CAN5_STORAGE_FSFAT_LOG_DIR "LOG.%i", idx);
        snprintf(dpath, 32, CAN5_STORAGE_FSFAT_LOG_DIR "LOG.%i", idx + 1);
    }

    if (stat(spath, &sb) != 0) {
        // the file does not exist ?
        return;
    }

    if (!S_ISREG(sb.st_mode)) {
        // the file is not a regular file
        return;
    }

    rename(spath, dpath);
}

static void __log_rotate()
{
    // optimist
    for (int i = MAX_LOG_FILES - 1; i >= 0; i--) {
        __check_and_rename(i);
    }
}

static void logstream_log(const can5_logger_msg_t *msg)
{
    time_t now;

    // check if we can flush or log rotate
    now = can5_time(NULL);
    __flush_sd_log(now, true);
    if (__should_rotate_log(now)) {
        __log_close_file();
        __log_rotate();
        CAN5_ERR_CHECK(__log_open_file());
    }

    fwrite(msg->msg, sizeof(char), msg->len, __log_stream.log_file);
}

static bool is_active()
{
    return __log_stream.is_active;
}
