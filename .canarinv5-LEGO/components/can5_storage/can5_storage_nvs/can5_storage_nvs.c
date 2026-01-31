#include <dirent.h>
#include <can5_utils.h>
#include <nvs_flash.h>
#include "nvs.h"
#include "esp_log.h"
#include "can5_storage_nvs.h"
#include "can5_storagedriv.h"

#define TAG "STORAGE_NVS_DRIVER"

#if 1
#define TRACE_FUNC ESP_LOGI(TAG, "in -> %s() :%d", __FUNCTION__, __LINE__)
#else
#define TRACE_FUNC_STORAGE_NVS_DRIVER
#endif



typedef enum storage_nvs_status_e {
    STORAGE_NVS_STAT_UNINITD = 0,                              /**< Uninitialized */
    STORAGE_NVS_STAT_INITD,                                    /**< Initialized */
} storage_nvs_status_t;

typedef struct storage_nvs_hdl_s {
    volatile  storage_nvs_status_t status;
} storage_nvs_hdl_t;

static storage_nvs_hdl_t this = {
    .status = STORAGE_NVS_STAT_UNINITD,
};


/* ---------------------------------------------------------------------
 * Forward declarations
 -----------------------------------------------------------------------*/

/* --------------------------------------------------------------------- */

can5_err_t can5_nvs_init()
{
    TRACE_FUNC;

    this.status = STORAGE_NVS_STAT_INITD;

    return CAN5_SUCCESS;
}

can5_err_t can5_nvs_uninit()
{
    TRACE_FUNC;
    ESP_LOGI(TAG, "Un-initializing NVS...");
    //TODO: delete all heap storage
    this.status = STORAGE_NVS_STAT_UNINITD;
    return CAN5_SUCCESS;
}

can5_err_t can5_nvs_write_int(const char *tag, const char *key, const int32_t data)
{
    TRACE_FUNC;
    nvs_handle_t handle;

    VERIFY_SUCCESS(nvs_open(tag, NVS_READWRITE, &handle));
                              ;
    VERIFY_SUCCESS_SAFERETURN(nvs_set_i32(handle, key, data),
                              nvs_close(handle));
    VERIFY_SUCCESS_SAFERETURN(nvs_commit(handle),
                              nvs_close(handle));
    nvs_close(handle);

    return CAN5_SUCCESS;
}

can5_err_t can5_nvs_read_int(const char *tag, const char *key, int32_t *data)
{
    TRACE_FUNC;
    nvs_handle_t handle;


    VERIFY_SUCCESS(nvs_open(tag, NVS_READONLY, &handle));
    VERIFY_SUCCESS_SAFERETURN(nvs_get_i32(handle, key, data),
                              nvs_close(handle));
    nvs_close(handle);

    return CAN5_SUCCESS;
}

can5_err_t can5_nvs_remove_row(const char *tag, const char *key)
{
    TRACE_FUNC;
    nvs_handle_t handle;


    VERIFY_SUCCESS(nvs_open(tag, NVS_READWRITE, &handle));
    VERIFY_SUCCESS_SAFERETURN(nvs_erase_key(handle, key),
                              nvs_close(handle));
    VERIFY_SUCCESS_SAFERETURN(nvs_commit(handle),
                              nvs_close(handle));
    nvs_close(handle);

    return CAN5_SUCCESS;
}

can5_err_t can5_nvs_remove_tag(const char *tag)
{
    TRACE_FUNC;
    nvs_handle_t handle;

    // else remove from NVS
    VERIFY_SUCCESS(nvs_open(tag, NVS_READWRITE, &handle));
    VERIFY_SUCCESS_SAFERETURN(nvs_erase_all(handle),
                              nvs_close(handle));
    VERIFY_SUCCESS_SAFERETURN(nvs_commit(handle),
                              nvs_close(handle));
    nvs_close(handle);

    return CAN5_SUCCESS;
}