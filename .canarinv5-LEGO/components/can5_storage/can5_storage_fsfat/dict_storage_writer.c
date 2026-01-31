/**************************************************
 * Author: rmukhia
 * Creation Date: 14/7/22
 * Description: 
 **************************************************/

#include <string.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <stdio.h>
#include <malloc.h>
#include "esp_log.h"
#include "dict_storage_writer.h"
#include "can5_storagedriv.h"
#include "can5_storage_fsfat.h"
#include "cJSON.h"
#include "can5_utils.h"

#define CLOSE_FILE(file)        if (fclose(file)) return CAN5_STORAGE_ERR_FILESYSTEM
#define CHECK_KEY_LEN(key)      if (strlen(key) > CAN5_STORAGE_KEY_LEN_MAX) return CAN5_STORAGE_ERR_KEY_TOO_LONG


static const char *TAG = "STORAGE_DICT";

#if 0
#define TRACE_FUNC ESP_LOGI(TAG, "in -> %s() :%d", __FUNCTION__, __LINE__)
#else
#define TRACE_FUNC
#endif

/* ---------------------------------------------------------------------
 * Forward declarations
 -----------------------------------------------------------------------*/

typedef struct key_val_s {
    char *key;
    char *val;
    size_t size;
    TAILQ_ENTRY(key_val_s) te;
} key_val_t;

typedef TAILQ_HEAD(key_val_list_s, key_val_s) key_val_list_t;

typedef struct wear_leveling_s {
    cJSON *data;
    key_val_list_t kv_list;
    char tag[CAN5_STORAGE_TAG_LEN_MAX];
    bool dirty;
    bool read;
    TAILQ_ENTRY(wear_leveling_s) te;
} wear_leveling_t;

typedef TAILQ_HEAD(wear_leveling_list_t, wear_leveling_s) wear_leveling_list_t;

static wear_leveling_list_t wear_leveling_list = TAILQ_HEAD_INITIALIZER(wear_leveling_list);

static can5_err_t __tag_to_path(const char *tag, char *path);


static can5_err_t __read_key_val_from_json(const char *tag, key_val_list_t *list);
static can5_err_t __read_file_content_json(const char *tag, cJSON **json);

static can5_err_t __write_key_val_to_json(const char *tag, key_val_list_t *list);
static can5_err_t __write_file_content_json(const char *tag, cJSON *json);

static bool __should_wearlevel(const char *tag);

static can5_err_t __get_kv_list(key_val_list_t **current_list,
                                const char *tag,
                                key_val_list_t *temp_list,
                                bool *is_wearlevel,
                                wear_leveling_t **wl);

static wear_leveling_t * __get_wearlevel(const char *tag);

static void __free_key_val_list(key_val_list_t *list);

static key_val_t *__get_key_val(const key_val_list_t *list, const char *key);

/* ---------------------------------------------------------------------
 * Function definitions
 -----------------------------------------------------------------------*/

can5_err_t can5_fsfat_di_write(const char *tag, const char *key, const uint8_t *buf, size_t buf_len)
{
    TRACE_FUNC;
    wear_leveling_t *wl;
    bool is_wearlevel;
    key_val_list_t kv_list, *used_kv_list;
    key_val_t *kv_elem;

    CHECK_KEY_LEN(key);

    VERIFY_SUCCESS(__get_kv_list(&used_kv_list, tag, &kv_list, &is_wearlevel, &wl));

    // get our value which we are interested in
    kv_elem = __get_key_val(used_kv_list, key);

    if (!kv_elem) {
        // if the key don't exist create new key_val
        VERIFY_ALLOC(kv_elem, sizeof(key_val_t));
        kv_elem->key = strdup(key);
        TAILQ_INSERT_TAIL(used_kv_list, kv_elem, te);
    }
    else {
        // free existing val
        if (kv_elem->val) {
            free(kv_elem->val);
            kv_elem->val = NULL;
            kv_elem->size = 0;
        }
    }

    kv_elem->val = strndup((const char *) buf, buf_len);
    kv_elem->size = buf_len;


    if (is_wearlevel) {
        // in case of wear leveling, set this node to dirty
        wl->dirty = true;
        return CAN5_SUCCESS;
    }
    else {
        can5_err_t ret;
        // in case of direct write, write this back and free the list
        ret = __write_key_val_to_json(tag, &kv_list);
        // free the local key val list
        __free_key_val_list(&kv_list);
        return ret;
    }
}

can5_err_t can5_fsfat_di_read(const char *tag, const char *key, uint8_t *buf, size_t *buf_len)
{
    TRACE_FUNC;

    can5_err_t ret;
    wear_leveling_t *wl;
    bool is_wearlevel;
    key_val_list_t kv_list, *used_kv_list;
    key_val_t *kv_elem;

    ret = CAN5_SUCCESS;

    CHECK_KEY_LEN(key);

    VERIFY_SUCCESS(__get_kv_list(&used_kv_list, tag, &kv_list, &is_wearlevel, &wl));

    // get our value which we are interested in
    kv_elem = __get_key_val(used_kv_list, key);

    if (!kv_elem) {
        ret = CAN5_STORAGE_ERR_KEY_NOT_FOUND;
        goto done;
    }

    strncpy((char *)buf, kv_elem->val, kv_elem->size);
    buf[kv_elem->size] = '\0';

    if (buf_len) {
        *buf_len = kv_elem->size;
    }

done:
    if (!is_wearlevel) {
        // free this list if its local
        __free_key_val_list(used_kv_list);
    }
    return ret;
}

can5_err_t can5_fsfat_di_delete(const char *tag, const char *key)
{
    TRACE_FUNC;
    can5_err_t ret;
    wear_leveling_t *wl;
    bool is_wearlevel;
    key_val_list_t kv_list, *used_kv_list;
    key_val_t *kv_elem;

    ret = CAN5_SUCCESS;

    CHECK_KEY_LEN(key);

    VERIFY_SUCCESS(__get_kv_list(&used_kv_list, tag, &kv_list, &is_wearlevel, &wl));

    // find element of interest
    kv_elem = __get_key_val(used_kv_list, key);

    // could not find key
    if (!kv_elem) {
        ret = CAN5_STORAGE_ERR_KEY_NOT_FOUND;
        goto done;
    }

    // remove element
    TAILQ_REMOVE(used_kv_list, kv_elem, te);

    if (kv_elem->val) {
        free(kv_elem->val);
    }

    if (kv_elem->key) {
        free(kv_elem->key);
    }

    free(kv_elem);

    if (is_wearlevel) {
        // set dirty
        wl->dirty = true;
    }
    else {
        // or else write to file
        ret = __write_key_val_to_json(tag, &kv_list);
    }

done:

    if (!is_wearlevel) {
        // free local kval
        __free_key_val_list(&kv_list);
    }

    return ret;
}

can5_err_t can5_fsfat_di_remove_tag(const char *tag)
{
    TRACE_FUNC;
    char filename[CAN5_LIFO_FILENAME_MAX + 1] = { 0 };
    struct stat st;
    wear_leveling_t *wl;

    VERIFY_SUCCESS(__tag_to_path(tag, filename));

    if (stat(filename, &st) == -1) {
        return CAN5_STORAGE_ERR_TAG_NOT_FOUND;
    }

    if (S_ISDIR(st.st_mode)) {
        // dictionary does not use directories
        return CAN5_STORAGE_ERR_TAG_NOT_FOUND;
    }

    if (remove(filename) == -1) {
        return CAN5_STORAGE_ERR_FILESYSTEM;
    }

    // check the wear leveling struct and remove any data
    TAILQ_FOREACH(wl, &wear_leveling_list, te) {
        if (strncmp(wl->tag, tag, strlen(tag)) == 0) {
            __free_key_val_list(&wl->kv_list);
            wl->dirty = false;
            wl->read = false;
        }
    }

    return CAN5_SUCCESS;
}

can5_err_t can5_fsfat_di_commit()
{
    TRACE_FUNC;

    wear_leveling_t *wl;
    TAILQ_FOREACH(wl, &wear_leveling_list, te) {
        if (wl->dirty) {
            VERIFY_SUCCESS(__write_key_val_to_json(wl->tag, &wl->kv_list));
            wl->dirty = false;
        }
    }
    return CAN5_SUCCESS;
}

can5_err_t can5_fsfat_di_register_wearlevel_tag(const char *tag)
{
    TRACE_FUNC;

    wear_leveling_t *wl;
    TAILQ_FOREACH(wl, &wear_leveling_list, te) {
        if (strcmp(tag, wl->tag) == 0) {
            return CAN5_SUCCESS;
        }
    }

    VERIFY_ALLOC(wl, sizeof(wear_leveling_t));
    wl->dirty = false;
    wl->read = false;

    TAILQ_INIT(&wl->kv_list);

    strncpy(wl->tag, tag, strlen(tag));

    TAILQ_INSERT_TAIL(&wear_leveling_list, wl, te);

    ESP_LOGI(TAG, "registering %s for wear level..", tag);

    return CAN5_SUCCESS;
}

can5_err_t can5_fsfat_di_unregister_wearlevel()
{
    TRACE_FUNC;

    wear_leveling_t *wl, *wl_next;
    TAILQ_FOREACH_SAFE(wl, &wear_leveling_list, te, wl_next) {
        __free_key_val_list(&wl->kv_list);
        free(wl);
    }

    return CAN5_SUCCESS;
}
/* ---------------------------------------------------------------------
 * Private Functions
 -----------------------------------------------------------------------*/

static can5_err_t __read_key_val_from_json(const char *tag, key_val_list_t *list)
{
    TRACE_FUNC;
    can5_err_t ret;
    cJSON *json_root;
    cJSON *json_elem;
    char *key, *value;
    key_val_t *kv_elem;

    json_root = json_elem = NULL;
    ret = CAN5_SUCCESS;

    VERIFY_SUCCESS(__read_file_content_json(tag, &json_root));

    TRACE_FUNC;
    cJSON_ArrayForEach(json_elem, json_root) {
        key = value = NULL;
        if (json_elem->string != NULL) {
            key = strdup(json_elem->string);
            value = strdup(cJSON_GetStringValue(json_elem));
        }
        else {
            continue;
        }
        kv_elem = NULL;
        kv_elem = malloc(sizeof(key_val_t));
        TRACE_FUNC;
        if (!kv_elem) {
            __free_key_val_list(list);
            ret = CAN5_ERR_OUT_OF_HEAP_MEMORY;
            goto done;
        }
        TRACE_FUNC;

        kv_elem->key = key;
        kv_elem->val = value;
        kv_elem->size = strlen(value);

        TAILQ_INSERT_TAIL(list, kv_elem, te);
    }


done:
    if (json_root) {
        cJSON_Delete(json_root);
    }
    return ret;
}

static can5_err_t __read_file_content_json(const char *tag, cJSON **json)
{
    TRACE_FUNC;

    can5_err_t ret;
    FILE *file;
    size_t file_len;
    char *f_buf;
    cJSON *json_content;
    char filename[CAN5_LIFO_FILENAME_MAX + 1];

    ret = CAN5_SUCCESS;
    file = NULL;
    f_buf = NULL;
    file_len = 0;
    json_content = NULL;
    CLEAR_ARRAY(filename);

    VERIFY_SUCCESS(__tag_to_path(tag, filename));

    VERIFY_SUCCESS(can5_fsfat_read_file_len(filename, &file_len));

    file = fopen(filename, "r");
    if (!file) {
        ret = CAN5_STORAGE_ERR_FILE_NOT_FOUND;
        goto done;
    }

    f_buf = calloc(file_len + 5, sizeof(char));

    if (!f_buf) {
        ret = CAN5_ERR_OUT_OF_HEAP_MEMORY;
        goto done;
    }

    if (fread(f_buf, file_len, 1, file) != 1) {
        ret = CAN5_STORAGE_ERR_FILESYSTEM;
        goto done;
    }

    json_content = cJSON_Parse(f_buf);

    if (!json_content) {
        ret = CAN5_STORAGE_ERR_JSON;
        goto done;
    }

done:
    if (ret != CAN5_SUCCESS) {
        *json = NULL;
    } else {
        *json = json_content;
    }

    if (f_buf) free(f_buf);
    if (file) fclose(file);
    return ret;
}

static can5_err_t __write_key_val_to_json(const char *tag, key_val_list_t *list)
{
    TRACE_FUNC;

    cJSON *root;
    can5_err_t ret;
    key_val_t *kv_elem;

    root = NULL;
    ret = CAN5_SUCCESS;
    kv_elem = NULL;

    if (!(root = cJSON_CreateObject())) {
        ret = CAN5_ERR_OUT_OF_HEAP_MEMORY;
        goto done;
    }

    TAILQ_FOREACH(kv_elem, list, te) {
        if (!cJSON_AddStringToObject(root, kv_elem->key, kv_elem->val)) {
            ret = CAN5_ERR_OUT_OF_HEAP_MEMORY;
            goto done;
        }
    }

    ret = __write_file_content_json(tag, root);

done:
    if (root) {
        cJSON_Delete(root);
    }
    return ret;
}

static can5_err_t __write_file_content_json(const char *tag, cJSON *json)
{
    TRACE_FUNC;

    can5_err_t ret;
    FILE *file;
    char *output;
    size_t output_len;
    char filename[CAN5_LIFO_FILENAME_MAX + 1];

    ret = CAN5_SUCCESS;
    file = NULL;
    output = NULL;
    CLEAR_ARRAY(filename);

    VERIFY_SUCCESS(__tag_to_path(tag, filename));

    file = fopen(filename, "w");
    if (!file) {
        ret = CAN5_STORAGE_ERR_FILESYSTEM;
        goto done;
    }

    output = cJSON_Print(json);
    if (!output) {
        ret = CAN5_ERR_OUT_OF_HEAP_MEMORY;
        goto done;
    }

    output_len = strlen(output);

    if (1 != fwrite(output, output_len, 1, file)) {
        ret = CAN5_STORAGE_ERR_FILESYSTEM;
        goto done;
    }

done:
    if (output) free(output);
    if (file) fclose(file);
    return ret;
}

static can5_err_t __tag_to_path(const char *tag, char *path)
{
    TRACE_FUNC;
    if (strlen(tag) > CAN5_STORAGE_TAG_LEN_MAX) {
        return CAN5_STORAGE_ERR_TAG_TOO_LONG;
    }

    strcpy(path, CAN5_STORAGE_FSFAT_DICT_DIR);

    strcat(path, tag);

    return CAN5_SUCCESS;
}

static bool __should_wearlevel(const char *tag)
{
    TRACE_FUNC;

    wear_leveling_t *wl_curr;
    TAILQ_FOREACH(wl_curr, &wear_leveling_list, te) {
        if (strncmp(wl_curr->tag, tag, strlen(tag)) == 0) {
            return true;
        }
    }

    return false;
}

static can5_err_t __get_kv_list(key_val_list_t **current_list,
                                const char *tag,
                                key_val_list_t *temp_list,
                                bool *is_wearlevel,
                                wear_leveling_t **wl)
{
    TRACE_FUNC;

    *is_wearlevel = __should_wearlevel(tag);

    if (*is_wearlevel) {
        // use from memory
        *wl = __get_wearlevel(tag);
        if(!(*wl)) {
            return CAN5_STORAGE_ERR_TAG_NOT_FOUND;
        }

        if (!(*wl)->read) {
            can5_err_t ret = __read_key_val_from_json(tag, &(*wl)->kv_list);
            if (ret == CAN5_SUCCESS || ret == CAN5_STORAGE_ERR_FILE_NOT_FOUND) {
                (*wl)->read = true;
            }
            else {
                VERIFY_SUCCESS(ret);
            }
        }
        *current_list = &(*wl)->kv_list;
    }
    else {
        // load from file
        TAILQ_INIT(temp_list);
        can5_err_t ret = __read_key_val_from_json(tag, temp_list);
        if (ret != CAN5_SUCCESS && ret != CAN5_STORAGE_ERR_FILE_NOT_FOUND) {
            VERIFY_SUCCESS(ret);
        }
        *current_list = temp_list;
    }

    return CAN5_SUCCESS;
}


static wear_leveling_t * __get_wearlevel(const char *tag)
{
    TRACE_FUNC;

    wear_leveling_t *wl_curr;
    TAILQ_FOREACH(wl_curr, &wear_leveling_list, te) {
        if (strncmp(wl_curr->tag, tag, strlen(tag)) == 0) {
            return wl_curr;
        }
    }

    return NULL;
}

static void __free_key_val_list(key_val_list_t *list)
{
    key_val_t *kv_elem, *tmp;
    TAILQ_FOREACH_SAFE(kv_elem, list, te, tmp) {
        if (kv_elem->val) {
            free(kv_elem->val);
            kv_elem->val = NULL;
        }
        if (kv_elem->key) {
            free(kv_elem->key);
            kv_elem->key = NULL;
        }
        free(kv_elem);
    };
}

static key_val_t *__get_key_val(const key_val_list_t *list, const char *key)
{
    key_val_t *kv_elem;
    TAILQ_FOREACH(kv_elem, list, te) {
        if (strcmp(kv_elem->key, key) == 0) {
            return kv_elem;
        }
    };

    return NULL;

}
