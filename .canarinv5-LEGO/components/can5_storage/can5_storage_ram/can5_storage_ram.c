/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   02/02/22
*
* File:  can5_storage_ram.c
* Descr:
*******************************************************************************/

#include <malloc.h>
#include <memory.h>
#include <esp_log.h>
#include <sys/queue.h>
#include "can5_storage_ram.h"
#include "can5_utils.h"

#define CAN5_STORAGE_RAM_USAGE_MAX  (1024 * 20)  // 20 kb
#define CHECK_INITD()               if (__ram_hdl.status == STORAGE_RAM_STAT_UNINITD) return CAN5_ERR_INVALID_STATE
#define CHECK_KEY_LEN(key)          if (strlen(key) > CAN5_STORAGE_KEY_LEN_MAX) return CAN5_STORAGE_ERR_KEY_TOO_LONG

#define STACK_COUNT                 4

//const static char *TAG = "STORAGE_RAM";


typedef struct tq_element_s {
    char tag[CAN5_STORAGE_TAG_LEN_MAX + 1];
    char key[CAN5_STORAGE_KEY_LEN_MAX + 1];
    uint8_t *val;
    size_t len;
    TAILQ_ENTRY(tq_element_s) te;
} tq_element_t;

typedef TAILQ_HEAD(tq_head_s, tq_element_s) tq_head_t;

typedef struct list_s {
    tq_head_t head;
    size_t members;
    size_t mem_size;
} list_t;


typedef enum storage_ram_status_e {
    STORAGE_RAM_STAT_UNINITD = 0,
    STORAGE_RAM_STAT_INITD,
    STORAGE_RAM_STAT_BUSY,
} storage_ram_status_t;

struct {
    volatile storage_ram_status_t status;
    list_t config_list;

    // handle upto four stacks
    list_t stack_list[STACK_COUNT];
    char stack_tag[STACK_COUNT][CAN5_STORAGE_TAG_LEN_MAX];
} __ram_hdl = {
    .status = STORAGE_RAM_STAT_UNINITD,
};


/* ---------------------------------------------------------------------
 * Forward declarations
 -----------------------------------------------------------------------*/

static can5_err_t  __peek_or_pop(const char *tag, uint8_t *data, size_t *data_len, const bool pop);
static int __get_stack_index(const char *tag, const bool acquire);
static size_t __get_ram_usage();
static void __free_list(list_t *list);
/* ---------------------------------------------------------------------
 * Function definitions
 -----------------------------------------------------------------------*/

can5_err_t can5_ram_init()
{
    if (__ram_hdl.status == STORAGE_RAM_STAT_INITD) {
        return CAN5_SUCCESS;
    }

    /* set everything to 0 */
    memset (&__ram_hdl, 0 , sizeof(__ram_hdl));

    TAILQ_INIT(&__ram_hdl.config_list.head);

    for (int i = 0; i < STACK_COUNT; i++) {
        TAILQ_INIT(&__ram_hdl.stack_list[i].head);
    }

    __ram_hdl.status = STORAGE_RAM_STAT_INITD;
    return CAN5_SUCCESS;
}

can5_err_t can5_ram_uninit()
{
    CHECK_INITD();

    __free_list(&__ram_hdl.config_list);

    for (int i =0 ; i< STACK_COUNT; i ++) {
        __free_list(&__ram_hdl.stack_list[i]);
    }

    memset(__ram_hdl.stack_tag, 0, sizeof (__ram_hdl.stack_tag));

    __ram_hdl.status = STORAGE_RAM_STAT_UNINITD;

    return CAN5_SUCCESS;
}

can5_err_t can5_ram_write(const char *tag, const char *key, const uint8_t *data, const size_t data_len)
{
    CHECK_INITD();
    CHECK_KEY_LEN(key);

    list_t *list = &__ram_hdl.config_list;
    tq_element_t *elem;

    if (__get_ram_usage() + data_len > CAN5_STORAGE_RAM_USAGE_MAX) {
        return CAN5_STORAGE_ERR_NO_SPACE;
    }

    // if the element already exists
    TAILQ_FOREACH(elem, &list->head, te) {
        if (!strcmp(tag, elem->tag) && !strcmp(key, elem->key)) {
            if (elem->len) {
                free(elem->val);
            }
            if (data_len) {
                VERIFY_ALLOC(elem->val, data_len);
                elem->len = data_len;
                memcpy(elem->val, data, data_len);
            }
            return CAN5_SUCCESS;
        }
    }

    // if the element does not exist
    VERIFY_ALLOC(elem, sizeof(tq_element_t));

    if (data_len) {
        VERIFY_ALLOC_SAFERETURN(elem->val, data_len, {
            free(elem);
        });
        memcpy(elem->val, data, data_len);
    }

    elem->len = data_len;

    strcpy(elem->tag, tag);

    strcpy(elem->key , key);


    TAILQ_INSERT_TAIL(&list->head, elem, te);
    list->members += 1;
    list->mem_size += data_len + sizeof(tq_element_t);

    return CAN5_SUCCESS;
}

can5_err_t can5_ram_read(const char *tag, const char *key, uint8_t *data, size_t *data_len)
{
    CHECK_INITD();
    CHECK_KEY_LEN(key);

    list_t *list = &__ram_hdl.config_list;
    tq_element_t *elem;

    TAILQ_FOREACH(elem, &list->head, te) {
        if (!strcmp(tag, elem->tag) && !strcmp(key, elem->key)) {
            if (data) {
                if (elem->len) {
                    memcpy(data, elem->val, elem->len);
                }
                data[elem->len] = '\0';
            }
            if (data_len) {
                *data_len = elem->len;
            }
            return CAN5_SUCCESS;
        }

    }

    return CAN5_STORAGE_ERR_INVALID_INDEX;
}

can5_err_t can5_ram_remove(const char *tag, const char *key)
{
    CHECK_INITD();
    CHECK_KEY_LEN(key);

    bool updated;

    tq_element_t *elem;
    list_t *list = &__ram_hdl.config_list;

    if (TAILQ_EMPTY(&list->head)) {
        return CAN5_STORAGE_ERR_INVALID_INDEX;
    }


    updated = false;
    TAILQ_FOREACH(elem, &list->head, te) {
        if (!strcmp(tag, elem->tag) && !strcmp(key, elem->key)) {
            updated = true;
            break;
        }
    }

    if (updated) {

        list->mem_size -= (elem->len + sizeof (tq_element_t));
        list->members -= 1;

        TAILQ_REMOVE(&list->head, elem, te);

        if (elem->len) {
            free(elem->val);
        }
        free(elem);

        return CAN5_SUCCESS;
    }

    return CAN5_STORAGE_ERR_INVALID_INDEX;
}

can5_err_t can5_ram_push(const char *tag, const uint8_t *data, const size_t data_len)
{
    CHECK_INITD();

    int stack_idx;
    tq_element_t *elem;
    list_t *list;

    stack_idx = __get_stack_index(tag, true);

    if (stack_idx == -1) {
        return CAN5_STORAGE_ERR_NO_SPACE;
    }

    list = &__ram_hdl.stack_list[stack_idx];

    if (__get_ram_usage() > CAN5_STORAGE_RAM_USAGE_MAX) {
        return CAN5_STORAGE_ERR_NO_SPACE;
    }

    VERIFY_ALLOC(elem, sizeof(tq_element_t));

    VERIFY_ALLOC_SAFERETURN(elem->val, data_len, {
        free(elem);
    });

    memcpy(elem->val, data, data_len);
    elem->len = data_len;
    // no need to set tag and key

    // add to tail
    TAILQ_INSERT_TAIL(&list->head, elem, te);
    list->members += 1;
    list->mem_size += data_len + sizeof(tq_element_t);

    return CAN5_SUCCESS;
}

can5_err_t can5_ram_pop(const char *tag, uint8_t *data, size_t *data_len)
{
    CHECK_INITD();
    return __peek_or_pop(tag, data, data_len, true);

}

can5_err_t can5_ram_peek(const char *tag, uint8_t *data, size_t *data_len)
{
    CHECK_INITD();
    return __peek_or_pop(tag, data, data_len, false);
}


can5_err_t can5_ram_remove_tag(const char *tag)
{
    CHECK_INITD();
    //dict_elem_t *elem, *prev_elem, *next_elem;
    tq_element_t *elem, *next;
    int stack_idx;


    TAILQ_FOREACH_SAFE(elem, &__ram_hdl.config_list.head, te, next) {
        if (!strcmp(tag, elem->tag)) {
            TAILQ_REMOVE(&__ram_hdl.config_list.head, elem, te);
            __ram_hdl.config_list.members -= 1;
            __ram_hdl.config_list.mem_size -= (elem->len + sizeof(tq_element_t));
            if (elem->len) {
                free(elem->val);
            }
            free(elem);
        }
    }

    stack_idx = __get_stack_index(tag, false);
    // no tag in stack
    if (stack_idx == -1) {
        return CAN5_SUCCESS;
    }

    __free_list(&__ram_hdl.stack_list[stack_idx]);

    CLEAR_ARRAY(__ram_hdl.stack_tag[stack_idx]);



    return CAN5_SUCCESS;
}


/* ---------------------------------------------------------------------
 * Private functions definitions
 -----------------------------------------------------------------------*/

static can5_err_t  __peek_or_pop(const char *tag, uint8_t *data, size_t *data_len, const bool pop)
{
    int stack_idx;
    tq_element_t *elem;
    list_t *list;

    stack_idx = __get_stack_index(tag, false);

    if (stack_idx == -1) {
        return CAN5_STORAGE_ERR_TAG_NOT_FOUND;
    }

    list = &__ram_hdl.stack_list[stack_idx];

    if (TAILQ_EMPTY(&list->head)) {
        return CAN5_STORAGE_ERR_EMPTY;
    }

    elem = TAILQ_LAST(&list->head, tq_head_s);

    memcpy(data, elem->val, elem->len);
    data[elem->len] = '\0';
    if (data_len) {
        *data_len = elem->len;
    }

    if (pop) {
        TAILQ_REMOVE(&list->head, elem, te);
        list->mem_size -= (sizeof(tq_element_t) + elem->len);
        list->members -= 1;
        if (elem->len) {
            free(elem->val);
        }
        free(elem);
    }

    return CAN5_SUCCESS;
}


static int __get_stack_index(const char *tag, const bool acquire)
{
    for (int i = 0; i < STACK_COUNT; i++ ) {
        if (!strcmp(tag, __ram_hdl.stack_tag[i])) {
            return i;
        }
    }

    if (acquire) {
        for (int i = 0; i < STACK_COUNT; i++ ) {
            list_t *list;
            list = &__ram_hdl.stack_list[i];
            if (TAILQ_EMPTY(&list->head) && !list->members) {
                strcpy(__ram_hdl.stack_tag[i], tag);
                return i;
            }
        }
    }

    return -1;
}

static size_t __get_ram_usage()
{
    size_t size;
    size = 0;
    for (int i =0 ; i < STACK_COUNT; i ++) {
        size += __ram_hdl.stack_list[i].mem_size;
    }
    size += __ram_hdl.config_list.mem_size;
    return size;
}

static void __free_list(list_t *list) {
    tq_element_t *elem, *next;

    TAILQ_FOREACH_SAFE(elem, &list->head, te, next) {
        TAILQ_REMOVE(&list->head, elem, te);
        if (elem->len) {
            free(elem->val);
        }
        free(elem);
    }

    list->members = 0;
    list->mem_size = 0;
}