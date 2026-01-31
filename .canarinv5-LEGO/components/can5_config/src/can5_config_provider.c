/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   18/01/22
*
* File:  can5_config_provider.c
* Descr:
*******************************************************************************/
#include <esp_log.h>
#include <esp_system.h>
#include <string.h>
#include <stdlib.h>
#include "can5_utils.h"
#include "can5_config_provider.h"
#include "can5_config_types.h"
#include "can5_storagemng.h"

static const char *COUNTER_TAG = "counter";


/* ---------------------------------------------------------------------
 * Forward declarations
 -----------------------------------------------------------------------*/

can5_err_t update_counter(const char *key, const bool increment)
{
    char val[32];
    int64_t counter;
    CLEAR_ARRAY(val);

    VERIFY_SUCCESS(get_counter(key, &counter));

    if (increment) {
        counter += 1;
    }
    else {
        counter -= 1;
    }

    snprintf(val, 32, "%lld", counter);

    return can5_storage_write_fs(COUNTER_TAG, key, (uint8_t *)val, strlen(val));
}

can5_err_t get_counter(const char *key, int64_t *val)
{
    char str[32];
    int64_t counter;
    can5_err_t ret;
    CLEAR_ARRAY(str);

    counter = 0;
    if ((ret = can5_storage_read_fs(COUNTER_TAG, key, (uint8_t *)&str, NULL)) != CAN5_SUCCESS) {
        if (ret == CAN5_STORAGE_ERR_TAG_NOT_FOUND || ret == CAN5_STORAGE_ERR_KEY_NOT_FOUND) {
            counter = 0;
        }
        VERIFY_SUCCESS(ret);
    }
    else {
        counter = strtoll(str, NULL, 10);
    }

    *val = counter;

    return CAN5_SUCCESS;
}
/* ---------------------------------------------------------------------
 * Function definition
 -----------------------------------------------------------------------*/

can5_err_t config_fs_init()
{
    const char **dirs;
    size_t len;
    const can5_cfg_meta_tags_t *tags;

    // create directories for files
    dirs = can5_get_cfg_raw_files_list(&len);
    for (int i = 0; i < len ; i++) {
        char *dir = strdup(dirs[i]);

        // get the directory
        for (int j = strlen(dir) - 1; j >= 0; j--) {
            if (dir[j] == '/') {
                dir[j + 1] = '\0';
                break;
            }
        }
        CAN5_ERR_CHECK_NO_ABORT(can5_storage_mkdir_p(dir));
        free(dir);
    }

    // add tags to wear-level api

    tags = can5_get_cfg_tag_list(&len);

    for (int i = 0; i < len; i++) {
        if (tags[i].wear_leveling) {
            can5_storage_register_wearlevel_tag(tags[i].tag);
        }
    }
    return CAN5_SUCCESS;
}


can5_err_t commit_config_to_disk()
{
    return can5_storage_commit_dictionary_fs();
}


can5_err_t config_write(can5_cfg_type_t type, const uint8_t *data, const size_t len)
{
    const can5_cfg_meta_t *meta;
    const char *key;

    meta = can5_config_get_meta(type);

    if (!meta) {
        return CAN5_ERROR;
    }

    key = can5_config_get_key_str(type);

    if (!key) {
        return CAN5_ERROR;
    }

    switch (meta->storage_type) {

        case CAN5_CFG_STORAGE_TYPE_NVS:
            return CAN5_CFG_ERR_INVALID_VALUE;
        case CAN5_CFG_STORAGE_TYPE_TAG_FS:
            return can5_storage_write_fs(meta->tag, key, data, len);
        case CAN5_CFG_STORAGE_TYPE_RAW_FS:
            return can5_storage_write_file(meta->tag, data, len);
        case CAN5_CFG_STORAGE_TYPE_RAM:
            return can5_storage_write_ram(meta->tag, key, data, len);
        default:
            break;
    }

    return CAN5_ERROR;
}

can5_err_t config_read(can5_cfg_type_t type, uint8_t **data, size_t *len)
{
    const can5_cfg_meta_t *meta;
    const char *key;
    size_t file_len;

    meta = can5_config_get_meta(type);

    if (!meta) {
        return CAN5_ERROR;
    }

    key = can5_config_get_key_str(type);

    if (!key) {
        return CAN5_ERROR;
    }

    *data = NULL;

    switch (meta->storage_type) {

        case CAN5_CFG_STORAGE_TYPE_NVS:
            return CAN5_CFG_ERR_INVALID_VALUE;
        case CAN5_CFG_STORAGE_TYPE_TAG_FS:
            VERIFY_ALLOC(*data, meta->max_len);
            return can5_storage_read_fs(meta->tag, key, *data, len);
        case CAN5_CFG_STORAGE_TYPE_RAW_FS:
            // get the file length
            VERIFY_SUCCESS(can5_storage_read_file_len(meta->tag, &file_len));
            // allocate memory
            // read from file
            if (file_len == 0) {
                /* Pass the heap comprehensive test */
                VERIFY_ALLOC(*data, 16);
                if (len) *len = 0;
                return CAN5_SUCCESS;
            }
            else {
                VERIFY_ALLOC(*data, file_len + 1);
                if (len) *len = file_len + 1;
                return can5_storage_read_file(meta->tag, *data, file_len);
            }

        case CAN5_CFG_STORAGE_TYPE_RAM:
            VERIFY_ALLOC(*data, meta->max_len);
            return can5_storage_read_ram(meta->tag, key, *data, len);
        default:
            break;
    }

    return CAN5_ERROR;
}

can5_err_t config_write_bool(can5_cfg_type_t type, bool data)
{
    const can5_cfg_meta_t *meta;
    const char *key;
    const char *c_data;
    size_t len;

    if (data) {
        c_data = CAN5_TRUE_TOKEN;
    }
    else {
        c_data = CAN5_FALSE_TOKEN;
    }

    len = strlen(c_data);

    meta = can5_config_get_meta(type);

    if (!meta) {
        return CAN5_ERROR;
    }

    key = can5_config_get_key_str(type);

    if (!key) {
        return CAN5_ERROR;
    }

    switch (meta->storage_type) {

        case CAN5_CFG_STORAGE_TYPE_NVS:
            return can5_storage_write_int_nvs(meta->tag, key, (int32_t)data);
        case CAN5_CFG_STORAGE_TYPE_TAG_FS:
            return can5_storage_write_fs(meta->tag, key, (uint8_t *)c_data, len);
        case CAN5_CFG_STORAGE_TYPE_RAW_FS:
            return can5_storage_write_file(meta->tag, (uint8_t *)c_data, len);
        case CAN5_CFG_STORAGE_TYPE_RAM:
            return can5_storage_write_ram(meta->tag, key, (uint8_t *)c_data, len);
        default:
            break;
    }

    return CAN5_ERROR;
}

can5_err_t config_read_bool(can5_cfg_type_t type, bool *data)
{
    const can5_cfg_meta_t *meta;
    const char *key;
    can5_err_t  ret;
    char *c_data;
    int32_t i_data;
    size_t len;

    meta = can5_config_get_meta(type);

    if (!meta) {
        return CAN5_ERROR;
    }

    key = can5_config_get_key_str(type);

    if (!key) {
        return CAN5_ERROR;
    }

    len = 0;
    ret = CAN5_SUCCESS;
    c_data = NULL;

    switch (meta->storage_type) {

        case CAN5_CFG_STORAGE_TYPE_NVS:
            VERIFY_ALLOC(c_data, meta->max_len);
            ret = can5_storage_read_int_nvs(meta->tag, key, &i_data);
            *data = i_data;
            goto end;
        case CAN5_CFG_STORAGE_TYPE_TAG_FS:
            VERIFY_ALLOC(c_data, meta->max_len);
            ret = can5_storage_read_fs(meta->tag, key, (uint8_t *)c_data, &len);
            break;
        case CAN5_CFG_STORAGE_TYPE_RAW_FS:
            VERIFY_SUCCESS(can5_storage_read_file_len(meta->tag, &len));
            VERIFY_ALLOC(c_data, len + 1);
            ret = can5_storage_read_file(meta->tag, (uint8_t *)c_data, len);
            break;
        case CAN5_CFG_STORAGE_TYPE_RAM:
            VERIFY_ALLOC(c_data, meta->max_len);
            ret = can5_storage_read_ram(meta->tag, key, (uint8_t *)c_data, &len);
            break;
        default:
            ret = CAN5_ERR_INVALID_PARAM;
            break;
    }

    if (ret != CAN5_SUCCESS) {
        goto end;
    }

    if (strncmp(c_data, CAN5_TRUE_TOKEN, len) == 0) {
        *data = true;
    }
    else if (strncmp(c_data, CAN5_FALSE_TOKEN, len) == 0) {
        *data = false;
    }
    else {
        ret = CAN5_CFG_ERR_INVALID_VALUE;
    }


end:
    if (c_data) {
        free(c_data);
    }
    return ret;

}

can5_err_t config_write_int(can5_cfg_type_t type, int64_t data)
{
    const can5_cfg_meta_t *meta;
    const char *key;
    char *c_data;
    size_t len;
    can5_err_t ret;

    len = snprintf(NULL, 0, "%lld", data);

    VERIFY_ALLOC(c_data, len + 1);

    snprintf(c_data, len + 1, "%lld", data);

    len = strlen(c_data);

    meta = can5_config_get_meta(type);

    if (!meta) {
        return CAN5_ERROR;
    }

    key = can5_config_get_key_str(type);

    if (!key) {
        return CAN5_ERROR;
    }

    ret = CAN5_ERROR;

    switch (meta->storage_type) {

        case CAN5_CFG_STORAGE_TYPE_NVS:
            ret = can5_storage_write_int_nvs(meta->tag, key, (int32_t) data);
            break;
        case CAN5_CFG_STORAGE_TYPE_TAG_FS:
            ret = can5_storage_write_fs(meta->tag, key, (uint8_t *)c_data, len);
            break;
        case CAN5_CFG_STORAGE_TYPE_RAW_FS:
            ret = can5_storage_write_file(meta->tag, (uint8_t *)c_data, len);
            break;
        case CAN5_CFG_STORAGE_TYPE_RAM:
            ret = can5_storage_write_ram(meta->tag, key, (uint8_t *)c_data, len);
            break;
        default:
            break;
    }

    free(c_data);

    return ret;
}

can5_err_t config_read_int(can5_cfg_type_t type, int64_t *data)
{
    const can5_cfg_meta_t *meta;
    const char *key;
    can5_err_t  ret;
    char *c_data;
    int32_t i_data;
    size_t len;

    meta = can5_config_get_meta(type);

    if (!meta) {
        return CAN5_ERROR;
    }

    key = can5_config_get_key_str(type);

    if (!key) {
        return CAN5_ERROR;
    }

    len = 0;
    ret = CAN5_SUCCESS;
    c_data = NULL;

    switch (meta->storage_type) {

        case CAN5_CFG_STORAGE_TYPE_NVS:
            VERIFY_ALLOC(c_data, meta->max_len);
            ret = can5_storage_read_int_nvs(meta->tag, key, &i_data);
            *data = i_data;
            goto end;
        case CAN5_CFG_STORAGE_TYPE_TAG_FS:
            VERIFY_ALLOC(c_data, meta->max_len);
            ret = can5_storage_read_fs(meta->tag, key, (uint8_t *)c_data, &len);
            break;
        case CAN5_CFG_STORAGE_TYPE_RAW_FS:
            VERIFY_SUCCESS(can5_storage_read_file_len(meta->tag, &len));
            VERIFY_ALLOC(c_data, len + 1);
            ret = can5_storage_read_file(meta->tag, (uint8_t *)c_data, len);
            break;
        case CAN5_CFG_STORAGE_TYPE_RAM:
            VERIFY_ALLOC(c_data, meta->max_len);
            ret = can5_storage_read_ram(meta->tag, key, (uint8_t *)c_data, &len);
            break;
        default:
            ret = CAN5_ERR_INVALID_PARAM;
            break;
    }

    if (ret != CAN5_SUCCESS) {
        goto end;
    }

    *data = strtoll(c_data, NULL, 10);

end:
    free(c_data);
    return ret;
}

can5_err_t config_write_double(can5_cfg_type_t type, double data)
{
    const can5_cfg_meta_t *meta;
    const char *key;
    char *c_data;
    size_t len;
    can5_err_t ret;

    len = snprintf(NULL, 0, "%f", data);

    VERIFY_ALLOC(c_data, len + 1);

    snprintf(c_data, len + 1, "%f", data);

    len = strlen(c_data);

    meta = can5_config_get_meta(type);

    if (!meta) {
        return CAN5_ERROR;
    }

    key = can5_config_get_key_str(type);

    if (!key) {
        return CAN5_ERROR;
    }

    ret = CAN5_ERROR;

    switch (meta->storage_type) {

        case CAN5_CFG_STORAGE_TYPE_NVS:
            ret = CAN5_CFG_ERR_INVALID_VALUE;
            break;
        case CAN5_CFG_STORAGE_TYPE_TAG_FS:
            ret = can5_storage_write_fs(meta->tag, key, (uint8_t *)c_data, len);
            break;
        case CAN5_CFG_STORAGE_TYPE_RAW_FS:
            ret = can5_storage_write_file(meta->tag, (uint8_t *)c_data, len);
            break;
        case CAN5_CFG_STORAGE_TYPE_RAM:
            ret = can5_storage_write_ram(meta->tag, key, (uint8_t *)c_data, len);
            break;
        default:
            break;
    }

    free(c_data);

    return ret;
}

can5_err_t config_read_double(can5_cfg_type_t type, double *data)
{
    const can5_cfg_meta_t *meta;
    const char *key;
    can5_err_t  ret;
    char *c_data;
    size_t len;

    meta = can5_config_get_meta(type);

    if (!meta) {
        return CAN5_ERROR;
    }

    key = can5_config_get_key_str(type);

    if (!key) {
        return CAN5_ERROR;
    }

    len = 0;
    ret = CAN5_SUCCESS;
    c_data = NULL;

    switch (meta->storage_type) {

        case CAN5_CFG_STORAGE_TYPE_NVS:
            ret = CAN5_CFG_ERR_INVALID_VALUE;
            break;
        case CAN5_CFG_STORAGE_TYPE_TAG_FS:
            VERIFY_ALLOC(c_data, meta->max_len);
            ret = can5_storage_read_fs(meta->tag, key, (uint8_t *)c_data, &len);
            break;
        case CAN5_CFG_STORAGE_TYPE_RAW_FS:
            VERIFY_SUCCESS(can5_storage_read_file_len(meta->tag, &len));
            VERIFY_ALLOC(c_data, len + 1);
            ret = can5_storage_read_file(meta->tag, (uint8_t *)c_data, len);
            break;
        case CAN5_CFG_STORAGE_TYPE_RAM:
            VERIFY_ALLOC(c_data, meta->max_len);
            ret = can5_storage_read_ram(meta->tag, key, (uint8_t *)c_data, &len);
            break;
        default:
            ret = CAN5_ERR_INVALID_PARAM;
            break;
    }

    if (ret != CAN5_SUCCESS) {
        goto end;
    }

    *data = strtod(c_data, NULL);

end:
    if (c_data) {
        free(c_data);
    }
    return ret;
}

can5_err_t config_remove(can5_cfg_type_t type)
{
    const can5_cfg_meta_t *meta;
    const char *key;

    meta = can5_config_get_meta(type);

    if (!meta) {
        return CAN5_ERROR;
    }

    key = can5_config_get_key_str(type);

    if (!key) {
        return CAN5_ERROR;
    }

    switch (meta->storage_type) {

        case CAN5_CFG_STORAGE_TYPE_NVS:
            return can5_storage_remove_nvs(meta->tag, key);
        case CAN5_CFG_STORAGE_TYPE_TAG_FS:
            return can5_storage_remove_fs(meta->tag, key);
        case CAN5_CFG_STORAGE_TYPE_RAM:
            return can5_storage_remove_ram(meta->tag, key);
        case CAN5_CFG_STORAGE_TYPE_RAW_FS:
            // TODO: how to remove fs
        default:
            break;
    }

    return CAN5_ERROR;
}

can5_err_t config_remove_tag(const char *tag)
{
    if (!tag) {
        return CAN5_ERROR;
    }

    can5_storage_remove_tag_nvs(tag);
    can5_storage_remove_tag_fs(tag);
    can5_storage_remove_tag_ram(tag);


    return CAN5_SUCCESS;
}

can5_err_t config_remove_file(const char *filename)
{
    if (!filename) {
        return CAN5_ERROR;
    }

    return can5_storage_remove_file(filename);
}