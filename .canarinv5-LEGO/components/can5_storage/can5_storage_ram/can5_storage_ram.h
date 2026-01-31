/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   02/02/22
*
* File:  can5_storage_ram.h
* Descr:
*******************************************************************************/

#ifndef TEST_APP_CAN5_STORAGE_RAM_H
#define TEST_APP_CAN5_STORAGE_RAM_H

#include <can5_storagedriv.h>

can5_err_t can5_ram_init();
can5_err_t can5_ram_push(const char *tag, const uint8_t *data, const size_t data_len);
can5_err_t can5_ram_pop(const char *tag, uint8_t *data, size_t *data_len);
can5_err_t can5_ram_peek(const char *tag, uint8_t *data, size_t *data_len);
can5_err_t can5_ram_write(const char *tag, const char *key, const uint8_t *data, const size_t data_len);
can5_err_t can5_ram_read(const char *tag, const char *key, uint8_t *data, size_t *data_len);
can5_err_t can5_ram_remove(const char *tag, const char *key);
can5_err_t can5_ram_remove_tag(const char *tag);
can5_err_t can5_ram_uninit();

#endif //TEST_APP_CAN5_STORAGE_RAM_H
