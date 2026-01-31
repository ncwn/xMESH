/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   03/02/22
*
* File:  can5_utils.c
* Descr:
*******************************************************************************/

#include <stdarg.h>
#include <malloc.h>
#include <stdint.h>
#include <ctype.h>
#include <esp_system.h>
#include <sys/time.h>
#include "can5_utils.h"
#include "can5_error.h"

const static char *__boolean_str[] = {
    "false",
    "true"
};

#define N_ARGS(args...) N_ARGS_HELPER1(args, 9, 8, 7, 6, 5, 4, 3, 2, 1)
#define N_ARGS_HELPER1(args...) N_ARGS_HELPER2(args)
#define N_ARGS_HELPER2(x1, x2, x3, x4, x5, x6, x7, x8, x9, n, x...) n

const char * _tag_lookup(uint32_t code, can5_tag_tab_t lut, size_t lut_size) {
    static const char* ret = "UNKNOWN";
    for (size_t t=0; t<lut_size/sizeof(can5_tag_item_t); t++) {
        if (lut[t].code == code) return lut[t].tag;
    }
    return ret;
}

void __free_bulk(int n, ...)
{
    va_list args;
    void *ptr;
    va_start(args, n);

    for (int i = 0; i < n; i++) {
        ptr = (void *)va_arg(args, intptr_t);

        if (ptr) {
            free(ptr);
        }
    }
    va_end(args);
}


const char *boolean_get_str(bool val)
{
    if (val) return __boolean_str[1];
    else return __boolean_str[0];
}

void remove_spaces(char* str) {
    char *write, *read;
    write = read = str;

    do {
        if (!isspace(*read)) {
            *write = *read;
            write++;
        }
        read++;
    } while (*read != '\0');

    *write = '\0';
}

static can5_shutdown_cb shutdown_cb = NULL;

can5_err_t can5_register_shutdown_handler(can5_shutdown_cb cb) {
    shutdown_cb = cb;
    return esp_register_shutdown_handler(cb);
}

can5_err_t can5_restart()
{
    VERIFY_SUCCESS(esp_sleep_enable_timer_wakeup(2 * 1000 * 1000));
    if (shutdown_cb) {
        shutdown_cb();
    }
    printf("........Shutting down CAN5.......\n");
    esp_deep_sleep_start();
    return CAN5_SUCCESS;
}

void can5_bin_to_hex(const uint8_t *bin, char *dst, size_t len)
{
    int i;
    char *dp;

    for (i = 0; i < len; i++) {
        dp = &dst[i * 2];
        snprintf(dp, 3, "%02x", bin[i]);
    }

}

static uint8_t __hexchar_to_bin2bytes(char ch)
{

    uint8_t byte = 0x00;

    switch (ch) {
        case '0':
            byte = 0x00;
            break;
        case '1':
            byte = 0x01;
            break;
        case '2':
            byte = 0x02;
            break;
        case '3':
            byte = 0x03;
            break;
        case '4':
            byte = 0x04;
            break;
        case '5':
            byte = 0x05;
            break;
        case '6':
            byte = 0x06;
            break;
        case '7':
            byte = 0x07;
            break;
        case '8':
            byte = 0x08;
            break;
        case '9':
            byte = 0x09;
            break;
        case 'A':
        case 'a':
            byte = 0x0A;
            break;
        case 'B':
        case 'b':
            byte = 0x0B;
            break;
        case 'C':
        case 'c':
            byte = 0x0C;
            break;
        case 'D':
        case 'd':
            byte = 0x0D;
            break;
        case 'E':
        case 'e':
            byte = 0x0E;
            break;
        case 'F':
        case 'f':
            byte = 0x0F;
            break;
        default:
            byte = 0xFF;
    }
    return byte;
}

int can5_hex_to_bin(const char *hex, void *bin, size_t len)
{
    int i;
    uint8_t byte, *ptr;

    //if (len % 2) /* hex string should have a length of multiple of two */
    //	return -1;

    for (i = 0; i < len * 2; i++) {
        ptr = (uint8_t *) bin + i / 2;
        byte = __hexchar_to_bin2bytes(hex[i]);

        if (!(i % 2))
            *ptr = byte << 4;
        else {
            *ptr = *ptr | byte;
        }
    }

    return 0;
}

int64_t time_ms(void *p)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000LL + (tv.tv_usec / 1000LL));
}
