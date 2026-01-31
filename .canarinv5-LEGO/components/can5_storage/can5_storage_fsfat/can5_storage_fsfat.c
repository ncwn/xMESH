#include <sys/stat.h>
#include <malloc.h>
#include <errno.h>
#include <sys/unistd.h>

#ifndef CAN5_LINUX_HOST_TEST
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include <sys/dirent.h>
#else
#include <dirent.h>
#endif

#include "can5_storage_fsfat.h"
#include "esp_log.h"
#include "can5_utils.h"
#include "lifo_storage_writer.h"
#include "dict_storage_writer.h"

static const char *TAG = "STORAGE_FSFAT_DRIVER";

#if 0
#define TRACE_FUNC ESP_LOGI(TAG, "in -> %s() :%d", __FUNCTION__, __LINE__)
#else
#define TRACE_FUNC
#endif

#define CHECK_BUSY  { if (__fsfat.status == STORAGE_FSFAT_STAT_BUSY) return CAN5_ERROR; } while(0)

typedef enum storage_fsfat_status_e {
    STORAGE_FSFAT_STAT_UNINITD = 0,                              /**< Uninitialized */
    STORAGE_FSFAT_STAT_INITD,                                    /**< Initialized */
    STORAGE_FSFAT_STAT_BUSY,                                     /**< BUSY */
} storage_fsfat_status_t;

struct storage_fsfat_hdl_s {
    volatile storage_fsfat_status_t status;
} __fsfat = {
    .status = STORAGE_FSFAT_STAT_UNINITD,
};

/* ---------------------------------------------------------------------
 * Forward declarations
 * --------------------------------------------------------------------- */

/* ---------------------------------------------------------------------
 * Function Definitions
 * --------------------------------------------------------------------- */

can5_err_t can5_fatfs_init()
{
    TRACE_FUNC;
    CHECK_BUSY;

    VERIFY_SUCCESS(can5_fsfat_make_dir_p(CAN5_STORAGE_FSFAT_STACK_DIR));
    VERIFY_SUCCESS(can5_fsfat_make_dir_p(CAN5_STORAGE_FSFAT_DICT_DIR));
    VERIFY_SUCCESS(can5_fsfat_make_dir_p(CAN5_STORAGE_FSFAT_LOG_DIR));


    __fsfat.status = STORAGE_FSFAT_STAT_INITD;

    return CAN5_SUCCESS;
}

can5_err_t can5_fsfat_uninit()
{
    // TODO: implement uninit
    return CAN5_SUCCESS;
}

can5_err_t can5_fsfat_remove_tag(const char *tag)
{
    TRACE_FUNC;
    can5_err_t ret;

    ret = can5_fsfat_di_remove_tag(tag);
    if (ret == CAN5_STORAGE_ERR_TAG_NOT_FOUND) {
        ret = can5_fsfat_lifo_remove_tag(tag);
    }

    return ret;
}

can5_err_t can5_fsfat_write_file(const char *filename, const uint8_t *data, size_t len)
{
    TRACE_FUNC;
    can5_err_t ret;
    FILE *file;

    ret = CAN5_SUCCESS;
    file = NULL;

    if (!data) {
        goto done;
    }

    file = fopen(filename, "wb");
    if (!file) {
        ret = CAN5_STORAGE_ERR_FILESYSTEM;
        goto done;
    }

    // if length is 0, then just create the file
    if (len == 0) {
        goto done;
    }

    if (1 != fwrite(data,len, 1, file)) {
        ret = CAN5_STORAGE_ERR_FILESYSTEM;
        goto done;
    }

done:
    if (file) {
        fclose(file);
    }

    return ret;
}

can5_err_t can5_fsfat_read_file_len(const char *filename, size_t *len)
{
    TRACE_FUNC;
    static struct stat st;

    errno = 0;
    if (stat(filename, &st) == -1) {
        ESP_LOGE(TAG, "Cannot read %s size: %s", filename, strerror(errno));
        return CAN5_STORAGE_ERR_FILE_NOT_FOUND;
    }

    *len = st.st_size;
    return CAN5_SUCCESS;
}

can5_err_t can5_fsfat_read_file(const char *filename, uint8_t *data, size_t len)
{
    TRACE_FUNC;
    can5_err_t ret;
    FILE *file;

    ret = CAN5_SUCCESS;
    file = NULL;

    if (!data) {
        goto done;
    }

    // if the length is 0, then return nothing
    if (len == 0) {
        goto done;
    }

    file = fopen(filename, "rb");
    if (!file) {
        ret = CAN5_STORAGE_ERR_FILESYSTEM;
        goto done;
    }

    if (fread(data,len, 1, file) != 1) {
        ret = CAN5_STORAGE_ERR_FILESYSTEM;
        goto done;
    }

done:
    if (file) {
        fclose(file);
    }

    return ret;
}

can5_err_t can5_fsfat_remove_file(const char *filename)
{
    errno = 0;
    if (0 != remove(filename)) {
        ESP_LOGE(TAG, "Cannot remove file %s: [%s]", filename, strerror(errno));
        return CAN5_STORAGE_ERR_FILESYSTEM;
    }

    return CAN5_SUCCESS;
}

can5_err_t can5_fsfat_make_dir_p(const char *path)
{
    TRACE_FUNC;
    struct stat st;
    char *npath = strdup(path); // duplicate on the stack

    ESP_LOGI(TAG, "make dir [%s]", npath);


    char *p = npath + 1;

    while ((p = strchr(p, '/'))) {
        *p = '\0';
        if (stat(npath, &st) == -1 ) {
            ESP_LOGI(TAG, "make dir [- %s]", npath);
            if (mkdir(npath, S_IRWXU)) {
                free(npath);
                ESP_LOGE(TAG, "make dir error %s", strerror(errno));
                return CAN5_STORAGE_ERR_FILESYSTEM;
            }
        }
        *p = '/';
        p++;
    }

    free(npath);

    return CAN5_SUCCESS;
}