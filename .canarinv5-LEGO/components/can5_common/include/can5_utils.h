/*******************************************************************************
 * Author: Luca De Mori @lucadm94
 * Date:   27-03-2020
 * 
 * File:  can5_utils.h
 * Descr: Serving function 
 *******************************************************************************/

#ifndef CAN5_UTIL_H
#define CAN5_UTIL_H

#include <stdint.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include <esp_sleep.h>
#include "esp_timer.h"
#include "can5_error.h"

//_______________________________________________________________________________________________________
//
//   VARIABLES INITIALIZATION
//-------------------------------------------------------------------------------------------------------
#define CLEAR_STRUCT(x) memset(&x, 0, sizeof(x))
#define CLEAR_ARRAY(x)  memset(x, 0, sizeof(x))

#define GET_BIT(pos, var)       ((var >> pos) & 0x01)
#define SET_BIT_HIGH(pos, var)  (var |= (0x01 << pos))
#define SET_BIT_LOW(pos, var)   (var &= ~(0x01 << pos))

// #define MAKESTRING(x) #x
// #define STRINGIFY(x)  MAKESTRING(x)

#define return_unlock(mutex, return_exp)  do {\
    xSemaphoreGive(mutex);                    \
    return return_exp;                               \
} while(0)

#define TAG_TAB_ITEM(err)    {err, #err}
#define TAG_LOOKUP(code, tag_table) _tag_lookup(code, tag_table, sizeof(tag_table))

typedef struct {
    uint32_t   code;
    const char *tag;
} can5_tag_item_t;

typedef const can5_tag_item_t can5_tag_tab_t[];

const char *boolean_get_str(bool val);
void remove_spaces(char* str);

const char * _tag_lookup(uint32_t code, can5_tag_tab_t lut, size_t lut_size);

#define min(a,b) (a<b?a:b)

#define can5_time(timer)     (time_t)(esp_timer_get_time() / 1000000)
#define can5_time_ms(timer)  (time_t)(esp_timer_get_time() / 1000)

#define can5_ms_to_sec(ms)   ((ms) / 1000)
#define can5_sec_to_ms(sec)  ((sec) * 1000)

int64_t time_ms(void *p);

#define __N_ARGS(args...) __N_ARGS_HELPER1(args, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1)
#define __N_ARGS_HELPER1(args...) __N_ARGS_HELPER2(args)
#define __N_ARGS_HELPER2(x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, n, x...) n

void __free_bulk(int n, ...);


#define FREE_BULK(args...) __free_bulk(__N_ARGS(args), args)


#if CONFIG_CAN5_SHOW_TASK_STACK_USAGE
#define PRINT_TASK_HIGHWATER_MARK(task) {                                               \
    UBaseType_t hwm;                                                                    \
    static int _pv_stack_ctr = 0;                                                       \
    if (_pv_stack_ctr == 0) {                                                           \
        hwm = uxTaskGetStackHighWaterMark(task);                                        \
        printf("\033[0;34m[%s] Stack Size left: %d \033[0m\n", pcTaskGetName(task), hwm);   \
    }                                                                                   \
    else {                                                                              \
    _pv_stack_ctr = (_pv_stack_ctr + 1) % 20;                                           \
    }}
#else
#define PRINT_TASK_HIGHWATER_MARK(task)
#endif


#if CONFIG_CAN5_SHOW_VERBOSE_LOG
#define ESP_LOGI_V(tag, ptr...) ESP_LOGI(tag, ptr)
#define ESP_LOGE_V(tag, ptr...) ESP_LOGE(tag, ptr)
#define ESP_LOGW_V(tag, ptr...) ESP_LOGW(tag, ptr)
#define ESP_LOG_BUFFER_HEXDUMP_V(TAG, bytes, len, level) ESP_LOG_BUFFER_HEXDUMP(TAG, bytes, len, level);
#else
#define ESP_LOGI_V(tag, ptr...)
#define ESP_LOGE_V(tag, ptr...)
#define ESP_LOGW_V(tag, ptr...)
#define ESP_LOG_BUFFER_HEXDUMP_V(TAG, bytes, len, level)
#endif

// /**
//  * @brief Find bytes in memory
//  * 
//  * @param __haystack pointer to search start address
//  * @param __haystack_size size of memory search space
//  * @param __needle   pointer to byte sequence to find
//  * @param __needle_size size of byte string to find
//  * @return void* 
//  */
// static void* memmem(const void *__haystack, size_t __haystack_size, const void * __needle, size_t __needle_size) {
//     if (!__haystack || !__needle || !__needle_size || __haystack_size<__needle_size) return NULL;

//     for (int p = 0; p<=__haystack_size-__needle_size; p++) {
//         if (memcmp(__haystack+p, __needle, __needle_size) == 0) return __haystack+p;
//     }

//     return NULL;

// }

/**
 * @brief Find string in memory space
 * 
 */
#define memstr(__haystack, __haystack_size, __needlestr) memmem(__haystack, __haystack_size, __needlestr, strlen(__needlestr))



//_______________________________________________________________________________________________________
//
//   FUNCTIONS DECLARATION
//-------------------------------------------------------------------------------------------------------


//_______________________________________________________________________________________________________
//
//   GLOBAL VARIABLES
//-------------------------------------------------------------------------------------------------------


//_______________________________________________________________________________________________________
//
//   FUNCTIONS DEFNITION
//-------------------------------------------------------------------------------------------------------

typedef void (*can5_shutdown_cb) (void);

can5_err_t can5_register_shutdown_handler(can5_shutdown_cb shutdown_cb);
can5_err_t can5_restart();

void can5_bin_to_hex(const uint8_t *bin, char *dst, size_t len);
int can5_hex_to_bin(const char *hex, void *bin, size_t len);

#endif // CAN5_UTIL_H