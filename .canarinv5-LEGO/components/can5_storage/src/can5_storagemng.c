#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "can5_storagemng.h"
#include "can5_events.h"
#include "can5_utils.h"

#include "can5_storage_fsfat.h"
#include "can5_storage_ram.h"
#include "can5_storage_nvs.h"
#include "dict_storage_writer.h"
//#include "can5_storage_sdspi.h"
//#include "can5_storage_nvs.h"

static const char *TAG = "STORAGE";

static const char *RAM_TEMP_TAG = "roll_tag";

/* 10 seconds to read a file */
#define FS_MAX_DELAY    pdMS_TO_TICKS(10000)
#define RAM_MAX_DELAY   pdMS_TO_TICKS(50)
#define NVS_MAX_DELAY   pdMS_TO_TICKS(100)

#if 0
#define TRACE_FUNC ESP_LOGI(TAG, "in -> %s() :%d", __FUNCTION__, __LINE__)
#else
#define TRACE_FUNC
#endif

/* --------------------------------------------------------------------- */

typedef enum can5_storage_status_e {
    CAN5_STORAGE_STAT_UNINITD,
    CAN5_STORAGE_STAT_INITD,
} can5_storage_status_t;

/* --------------------------------------------------------------------- */
typedef struct storage_dev_s {
    SemaphoreHandle_t sem;
} storage_dev_t;

struct storage_hdl_s {
    volatile can5_storage_status_t status;
    storage_dev_t fs;
    storage_dev_t nvs;
    storage_dev_t ram;
} __storage  = {
    .status = CAN5_STORAGE_STAT_UNINITD,
    .fs = {
        .sem = NULL,
    },
    .nvs = {
        .sem = NULL,
    },
    .ram = {
        .sem = NULL,
    },
};

ESP_EVENT_DEFINE_BASE(CAN5_EVT_STORAGEMNG);

/* ---------------------------------------------------------------------
 * Forward declarations
 -----------------------------------------------------------------------*/

can5_err_t can5_storage_init()
{
    TRACE_FUNC;

    if (__storage.status == CAN5_STORAGE_STAT_INITD) {
        return CAN5_SUCCESS;
    }

    VERIFY_SUCCESS(can5_fatfs_init());
    VERIFY_SUCCESS(can5_ram_init());
    VERIFY_SUCCESS(can5_nvs_init());


    VERIFY_NOT_NULL((__storage.fs.sem = xSemaphoreCreateMutex()));
    VERIFY_NOT_NULL((__storage.nvs.sem = xSemaphoreCreateMutex()));
    VERIFY_NOT_NULL((__storage.ram.sem = xSemaphoreCreateMutex()));



    __storage.status = CAN5_STORAGE_STAT_INITD;
    esp_event_post(CAN5_EVT_STORAGEMNG, CAN5_STORAGEMNG_EVT_INITIALIZED, NULL, 0, pdMS_TO_TICKS(1000));
    return CAN5_SUCCESS;
}

can5_err_t can5_storage_uninit()
{
    TRACE_FUNC;

    if (__storage.status == CAN5_STORAGE_STAT_UNINITD) {
        return CAN5_SUCCESS;
    }

    can5_fsfat_uninit();
    can5_ram_uninit();
    can5_nvs_uninit();

    // at least one driver got uninitialized.
    vSemaphoreDelete(__storage.fs.sem);
    vSemaphoreDelete(__storage.ram.sem);
    vSemaphoreDelete(__storage.nvs.sem);

    __storage.status = CAN5_STORAGE_STAT_UNINITD;
    return CAN5_SUCCESS;
}

int32_t can5_storage_status_get()
{
    TRACE_FUNC;

    return __storage.status;
}

#define RUN_SEM_PROTECTED(procedure, event, dev, timeout) {             \
    if (xSemaphoreTake((dev).sem, timeout) == pdTRUE) {                   \
        can5_err_t ret = procedure;                                     \
        xSemaphoreGive((dev).sem);                                        \
        if (ret == CAN5_SUCCESS) {                                      \
            event;                                                      \
        }                                                               \
        return ret;                                                     \
    }                                                                   \
    else {                                                              \
        return CAN5_STORAGE_ERR_BUSY;                                   \
    }                                                                   \
}
/* ---------------------------------------------------------------------
 * Filesystem
 -----------------------------------------------------------------------*/

can5_err_t can5_storage_push_fs(const char *tag, const uint8_t *data, size_t data_len)
{
    TRACE_FUNC;


    RUN_SEM_PROTECTED(
            can5_fsfat_lifo_push(tag, data, data_len),
            esp_event_post(CAN5_EVT_STORAGEMNG, CAN5_STORAGEMNG_EVT_PUSHED, NULL, 0, 10),
            __storage.fs,
            FS_MAX_DELAY
    );
}

can5_err_t can5_storage_pop_fs(const char *tag, uint8_t *data, size_t *data_len)
{
    TRACE_FUNC;

    RUN_SEM_PROTECTED(
            can5_fsfat_lifo_pop(tag, data, data_len),
            esp_event_post(CAN5_EVT_STORAGEMNG, CAN5_STORAGEMNG_EVT_POPPED, NULL, 0, 10),
            __storage.fs,
            FS_MAX_DELAY
    );
}

can5_err_t can5_storage_peek_fs(const char *tag, uint8_t *data, size_t *data_len)
{
    TRACE_FUNC;

    RUN_SEM_PROTECTED(
            can5_fsfat_lifo_peek(tag, data, data_len),,
            __storage.fs,
            FS_MAX_DELAY
    );
}

can5_err_t can5_storage_write_fs(const char *tag, const char *key, const uint8_t *data, size_t data_len)
{
    TRACE_FUNC;

    RUN_SEM_PROTECTED(
            can5_fsfat_di_write(tag, key, data, data_len),,
            __storage.fs,
            FS_MAX_DELAY
    );
}

can5_err_t can5_storage_read_fs(const char *tag, const char *key, uint8_t *data, size_t *data_len)
{
    TRACE_FUNC;

    RUN_SEM_PROTECTED(
            can5_fsfat_di_read(tag, key, data, data_len),,
            __storage.fs,
            FS_MAX_DELAY
    );
}


can5_err_t can5_storage_remove_fs(const char *tag, const char *key)
{
    TRACE_FUNC;

    RUN_SEM_PROTECTED(
            can5_fsfat_di_delete(tag, key),,
            __storage.fs,
            FS_MAX_DELAY
    );
}

can5_err_t can5_storage_commit_dictionary_fs()
{
    TRACE_FUNC;

    RUN_SEM_PROTECTED(
            can5_fsfat_di_commit(),,
            __storage.fs,
            FS_MAX_DELAY
    );
}

can5_err_t can5_storage_remove_tag_fs(const char *tag)
{
    TRACE_FUNC;

    RUN_SEM_PROTECTED(
            can5_fsfat_remove_tag(tag),,
            __storage.fs,
            FS_MAX_DELAY
    );
}

can5_err_t can5_storage_remove_old_data_fs(const char *tag)
{
    TRACE_FUNC;

    RUN_SEM_PROTECTED(
            can5_fsfat_lifo_remove_old_data(tag),,
            __storage.fs,
            FS_MAX_DELAY
    );
}

static uint8_t tmp_sp[CAN5_STORAGE_MAX_LEN];
can5_err_t can5_storage_search_and_pop_fs(const char *tag, const uint8_t *data, size_t data_len, size_t max_depth)
{
    TRACE_FUNC;
    size_t tmp_size;
    size_t depth;
    can5_err_t ret;

    ret = CAN5_STORAGE_ERR_SEARCH_FAIL;

    if (xSemaphoreTake(__storage.fs.sem, FS_MAX_DELAY) == pdTRUE) {

        for(depth = 0; depth < max_depth; depth++) {

            memset(&tmp_sp, 0, CAN5_STORAGE_MAX_LEN);
            tmp_size = 0;

            VERIFY_SUCCESS_SAFERETURN(ret = can5_fsfat_lifo_pop(tag, (uint8_t *) &tmp_sp, &tmp_size), {
                goto step2;
            });

            esp_event_post(CAN5_EVT_STORAGEMNG, CAN5_STORAGEMNG_EVT_POPPED, NULL, 0, 10);

            // ESP_LOGI(TAG, "matching %d %d and (%s) (%s)", data_len, tmp_size, data, tmp);
            if (data_len == tmp_size && memcmp(data, tmp_sp, data_len) == 0) {
                // we have found match
                ret = CAN5_SUCCESS;
                break;

            }

            VERIFY_SUCCESS_SAFERETURN(ret = can5_storage_push_ram(RAM_TEMP_TAG, tmp_sp, tmp_size), {
                goto step2;
            });
        }

step2:
        for (; depth > 0; depth--) {
            memset(&tmp_sp, 0, CAN5_STORAGE_MAX_LEN);
            tmp_size = 0;

            VERIFY_SUCCESS_SAFERETURN(ret = can5_storage_pop_ram(RAM_TEMP_TAG, (uint8_t  *)&tmp_sp, &tmp_size), {
                goto done;
            });

            VERIFY_SUCCESS_SAFERETURN(ret = can5_fsfat_lifo_push(tag, (uint8_t *) tmp_sp, tmp_size), {
                goto done;
            });

            esp_event_post(CAN5_EVT_STORAGEMNG, CAN5_STORAGEMNG_EVT_PUSHED, NULL, 0, 10);
        }
done:
        xSemaphoreGive(__storage.fs.sem);
    }
    else {
        ret = CAN5_STORAGE_ERR_BUSY;
    }
    TRACE_FUNC;

    return ret;
}

can5_err_t can5_storage_write_file(const char *filename, const uint8_t *data, size_t len)
{
    TRACE_FUNC;

    RUN_SEM_PROTECTED(
            can5_fsfat_write_file(filename, data, len),,
            __storage.fs,
            FS_MAX_DELAY
    );
}

can5_err_t can5_storage_read_file_len(const char *filename, size_t *len)
{
    TRACE_FUNC;

    RUN_SEM_PROTECTED(
            can5_fsfat_read_file_len(filename, len),,
            __storage.fs,
            FS_MAX_DELAY
    );
}

can5_err_t can5_storage_read_file(const char *filename, uint8_t *data, size_t len)
{
    TRACE_FUNC;

    RUN_SEM_PROTECTED(
            can5_fsfat_read_file(filename, data, len),,
            __storage.fs,
            FS_MAX_DELAY
    );
}

can5_err_t can5_storage_remove_file(const char *filename)
{
    TRACE_FUNC;

    RUN_SEM_PROTECTED(
            can5_fsfat_remove_file(filename),,
            __storage.fs,
            FS_MAX_DELAY
    );
}

can5_err_t can5_storage_mkdir_p(const char *dirname)
{
    TRACE_FUNC;

    RUN_SEM_PROTECTED(
            can5_fsfat_make_dir_p(dirname),,
            __storage.fs,
            FS_MAX_DELAY
    );
}

can5_err_t can5_storage_register_wearlevel_tag(const char *tag)
{
    TRACE_FUNC;

    RUN_SEM_PROTECTED(
            can5_fsfat_di_register_wearlevel_tag(tag),,
            __storage.fs,
            FS_MAX_DELAY
            );
}
/* ---------------------------------------------------------------------
 * RAM
 -----------------------------------------------------------------------*/
can5_err_t can5_storage_push_ram(const char *tag, const uint8_t *data, const size_t data_len)
{
    TRACE_FUNC;

    RUN_SEM_PROTECTED(
        can5_ram_push(tag, data, data_len),
        esp_event_post(CAN5_EVT_STORAGEMNG, CAN5_STORAGEMNG_EVT_PUSHED, NULL, 0, pdMS_TO_TICKS(100)),
        __storage.ram,
        RAM_MAX_DELAY
    );
}

can5_err_t can5_storage_pop_ram(const char *tag, uint8_t *data, size_t *data_len)
{
    TRACE_FUNC;

    RUN_SEM_PROTECTED(
        can5_ram_pop(tag, data, data_len),
        esp_event_post(CAN5_EVT_STORAGEMNG, CAN5_STORAGEMNG_EVT_POPPED, NULL, 0, pdMS_TO_TICKS(100)),
        __storage.ram,
        RAM_MAX_DELAY
    );
}

can5_err_t can5_storage_peek_ram(const char *tag, uint8_t *data, size_t *data_len)
{
    TRACE_FUNC;

    RUN_SEM_PROTECTED(
        can5_ram_peek(tag, data, data_len),,
        __storage.ram,
        RAM_MAX_DELAY
    );
}

can5_err_t can5_storage_write_ram(const char *tag, const char *key, const uint8_t *data, const size_t data_len)
{
    TRACE_FUNC;

    RUN_SEM_PROTECTED(
        can5_ram_write(tag, key, data, data_len),,
        __storage.ram,
        RAM_MAX_DELAY
    );
}

can5_err_t can5_storage_read_ram(const char *tag, const char *key, uint8_t *data, size_t *data_len)
{
    TRACE_FUNC;

    RUN_SEM_PROTECTED(
        can5_ram_read(tag, key, data, data_len),,
        __storage.ram,
        RAM_MAX_DELAY
    );
}

can5_err_t can5_storage_remove_ram(const char *tag, const char *key)
{
    TRACE_FUNC;

    RUN_SEM_PROTECTED(
        can5_ram_remove(tag, key),,
        __storage.ram,
        RAM_MAX_DELAY
    );
}

can5_err_t can5_storage_remove_tag_ram(const char *tag)
{
    TRACE_FUNC;

    RUN_SEM_PROTECTED(
        can5_ram_remove_tag(tag),,
        __storage.ram,
        RAM_MAX_DELAY
    );
}

/* ---------------------------------------------------------------------
 * Flash
 -----------------------------------------------------------------------*/
can5_err_t can5_storage_write_int_nvs(const char *tag, const char *key, int32_t data)
{
    TRACE_FUNC;

    RUN_SEM_PROTECTED(
        can5_nvs_write_int(tag, key, data),,
        __storage.nvs,
        NVS_MAX_DELAY
    );
}

can5_err_t can5_storage_read_int_nvs(const char *tag, const char *key, int32_t *data)
{
    TRACE_FUNC;

    RUN_SEM_PROTECTED(
        can5_nvs_read_int(tag, key, data),,
        __storage.nvs,
        NVS_MAX_DELAY
    );
}

can5_err_t can5_storage_remove_nvs(const char *tag, const char *key)
{
    TRACE_FUNC;
    RUN_SEM_PROTECTED(
        can5_nvs_remove_row(tag, key),,
        __storage.nvs,
        NVS_MAX_DELAY
    );
}

can5_err_t can5_storage_remove_tag_nvs(const char *tag)
{
    TRACE_FUNC;
    RUN_SEM_PROTECTED(
        can5_nvs_remove_tag(tag),,
        __storage.nvs,
        NVS_MAX_DELAY
    );
}

/* ---------------------------------------------------------------------
 * Private functions
 -----------------------------------------------------------------------*/

//_______________________________________________________________________________________________________
//
//   DEBUG SUPPORT
//-------------------------------------------------------------------------------------------------------

#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL>0

static const can5_tag_tab_t _storage_evt_tags = {
    TAG_TAB_ITEM(CAN5_STORAGEMNG_EVT_NONE ),
    TAG_TAB_ITEM(CAN5_STORAGEMNG_EVT_INITIALIZED ),
    TAG_TAB_ITEM(CAN5_STORAGEMNG_EVT_PUSHED ),
    TAG_TAB_ITEM(CAN5_STORAGEMNG_EVT_POPPED ),
};


const char* can5_storage_evt_getstr(int32_t evt) {
    TRACE_FUNC;

    return TAG_LOOKUP(evt, _storage_evt_tags);;
}

#endif
