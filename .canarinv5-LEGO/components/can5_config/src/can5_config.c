//
// Created by rmukhia on 23/12/21.
//

#include <string.h>
#include <stdlib.h>
#include <can5_config_types.h>
#include "can5_config.h"
#include <can5_utils.h>
#include "can5_config_provider.h"
#include "esp_log.h"
#include "esp_mac.h"


#if 0
#define TRACE_FUNC_CONFIG ESP_LOGI_V(TAG, "in -> %s() :%d", __FUNCTION__, __LINE__)
#else
#define TRACE_FUNC
#endif

typedef enum can5_config_status_e {
    CONFIG_STAT_UNINITD,
    CONFIG_STAT_INITD,

    CONFIG_STAT_LAST,
} can5_config_status_t;


/*************************************************************************************
 * Declarations
 *************************************************************************************/

static can5_err_t init();

static can5_err_t uninit();


static can5_err_t factory_default();

static bool is_sleepable();

static bool is_sleeping();

static int32_t status_get();

#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL > 0

static const char *status_getstr(int32_t status);

#endif

static can5_err_t factory_default();

can5_cfg_t config_manager = {
    .module = {
        .init = init,
        .uninit = uninit,
        .is_sleepable = is_sleepable,
        .is_sleeping = is_sleeping,
        .status_get = status_get,
#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL > 0
        .status_getstr = status_getstr,
#endif
    },

    .write = config_write,
    .read = config_read,

    .write_bool = config_write_bool,
    .read_bool = config_read_bool,

    .write_int = config_write_int,
    .read_int = config_read_int,

    .write_double = config_write_double,
    .read_double = config_read_double,

    .remove = config_remove,

    .update_counter = update_counter,
    .get_counter = get_counter,

    .factory_default = factory_default,

    .commit_config_to_disk = commit_config_to_disk,
};

struct can5_config_hdl_s {
    can5_config_status_t status;
} __config_hdl;

static const char *TAG = "CONFIG";

/*************************************************************************************
 * Module Ops
 *************************************************************************************/

static can5_err_t init()
{
    TRACE_FUNC;
    char *val;
    int64_t i_val;
    char *str_true = "true";
    const can5_cfg_meta_t *meta_list;
    can5_err_t result;
    size_t meta_list_len;
    bool is_set;

    // initialize raw fs directories. Tag dictionaries are handled by storage_manager.
    config_fs_init();

    is_set = false;
    // check is SET_FLAG is set
    config_manager.read_bool(CFG_INIT, &is_set);

    meta_list = can5_config_get_all_meta_elems(&meta_list_len);
    if (!is_set) {

        for (size_t i = 0; i < meta_list_len; i++) {
            const can5_cfg_meta_t *meta = &meta_list[i];
            if (meta->storage_type == CAN5_CFG_STORAGE_TYPE_RAW_FS) {
                continue;
            }

            /* Issue #57: The initial device_name should be can5-[last 3 octates of mac address.] */
            if (meta->type == CFG_DEVICE_NAME)
            {
                uint8_t mac[6];
#if CONFIG_IDF_TARGET_ESP32
                char mac_val[] = "can5-112233445566";
#elif CONFIG_IDF_TARGET_ESP32S3
                char mac_val[] = "can6-112233445566";
#endif


                /* Get mac_id */
                VERIFY_SUCCESS(esp_efuse_mac_get_default((uint8_t *)&mac));
                can5_bin_to_hex(mac, (char *)mac_val + 5, 6);
                strcpy(mac_val + 5, mac_val + 11);

                CAN5_ERR_CHECK_NO_ABORT(config_manager.write(CFG_DEVICE_NAME , (uint8_t  *)mac_val, 11));
                continue;
            }

            CAN5_ERR_CHECK_NO_ABORT(config_manager.write(meta->type, (uint8_t  *)meta->default_val,
                                 strlen(meta->default_val)));
        }

        VERIFY_SUCCESS(config_manager.write(CFG_INIT,
                                            (uint8_t *)str_true, strlen(str_true)));
    }

    for (size_t i = 0; i < meta_list_len; i++) {
        const can5_cfg_meta_t *meta = &meta_list[i];

        // do not process file based config
        //if (meta->storage_type == CAN5_CFG_STORAGE_TYPE_RAW_FS) {
        //    continue;
        //}

        val = NULL;
        // Write default values for RAM
        if (meta->storage_type == CAN5_CFG_STORAGE_TYPE_RAM) {
            CAN5_ERR_CHECK(config_manager.write(meta->type, (uint8_t  *)meta->default_val,
                                                strlen(meta->default_val)));
        }

        if (meta->storage_type == CAN5_CFG_STORAGE_TYPE_NVS) {
            CAN5_ERR_CHECK_NO_ABORT(result = config_manager.read_int(meta->type, &i_val));
        }
        else {
            CAN5_ERR_CHECK_NO_ABORT(result = config_manager.read(meta->type, (uint8_t **) &val, NULL));
        }
        /*
         * If the value we are trying to read is wrong then try to write default value.
         */
        if (result != CAN5_SUCCESS) {
            if (meta->storage_type == CAN5_CFG_STORAGE_TYPE_NVS) {
                CAN5_ERR_CHECK_NO_ABORT(config_manager.write_int(meta->type, (int32_t)strtol(meta->default_val, NULL, 10)));
                CAN5_ERR_CHECK_NO_ABORT(result = config_manager.read_int(meta->type, &i_val));
            }
            else {
                CAN5_ERR_CHECK_NO_ABORT(config_manager.write(meta->type, (uint8_t *) meta->default_val,
                                                             strlen(meta->default_val)));
                CAN5_ERR_CHECK_NO_ABORT(result = config_manager.read(meta->type, (uint8_t **) &val, NULL));
            }
        }

        if (meta->storage_type == CAN5_CFG_STORAGE_TYPE_NVS) {
            ESP_LOGI(TAG, "Config: %s:%lld", can5_config_get_key_str(meta_list[i].type), i_val);
        }
        else {
            ESP_LOGI(TAG, "Config: %s:%s", can5_config_get_key_str(meta_list[i].type), val);
        }

        if (val) {
            free(val);
        }

    }

    __config_hdl.status = CONFIG_STAT_INITD;

    return config_manager.commit_config_to_disk();
}

static can5_err_t uninit()
{
    TRACE_FUNC;
    __config_hdl.status = CONFIG_STAT_UNINITD;
    return CAN5_SUCCESS;
}

static can5_err_t factory_default()
{
    TRACE_FUNC;

    const can5_cfg_meta_tags_t *tags;
    const char **dirs;
    size_t len;

    tags = can5_get_cfg_tag_list(&len);

    for (size_t i = 0; i < len ;i++) {
        config_remove_tag(tags[i].tag);
    }

    dirs = can5_get_cfg_raw_files_list(&len);

    for (size_t i = 0; i < len ;i++) {
        config_remove_file(dirs[i]);
    }

    return CAN5_SUCCESS;
}

static bool is_sleepable()
{
    return true;
}

static bool is_sleeping()
{
    return true;
}

static int32_t status_get()
{
    TRACE_FUNC;
    return __config_hdl.status;
}


#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL > 0

static const char *status_getstr(int32_t status)
{
    TRACE_FUNC;
    return "OK";
}

#endif
