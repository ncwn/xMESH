#ifndef __CAN5_STORAGE_H__
#define __CAN5_STORAGE_H__

#include "can5_error.h"

can5_err_t can5_storage_init();
can5_err_t can5_storage_uninit();

can5_err_t can5_storage_push_fs(const char *tag, const uint8_t *data, size_t data_len);
can5_err_t can5_storage_pop_fs(const char *tag, uint8_t *data, size_t *data_len);
can5_err_t can5_storage_peek_fs(const char *tag, uint8_t *data, size_t *data_len);
can5_err_t can5_storage_write_fs(const char *tag, const char *key, const uint8_t *data, size_t data_len);
can5_err_t can5_storage_read_fs(const char *tag, const char *key, uint8_t *data, size_t *data_len);
can5_err_t can5_storage_remove_fs(const char *tag, const char *key);
can5_err_t can5_storage_remove_tag_fs(const char *tag);
can5_err_t can5_storage_remove_old_data_fs(const char *tag);
can5_err_t can5_storage_commit_dictionary_fs();
can5_err_t can5_storage_search_and_pop_fs(const char *tag, const uint8_t *data, size_t data_len, size_t max_depth);
can5_err_t can5_storage_write_file(const char *filename, const uint8_t *data, size_t len);
can5_err_t can5_storage_read_file_len(const char *filename, size_t *len);
can5_err_t can5_storage_read_file(const char *filename, uint8_t *data, size_t len);
can5_err_t can5_storage_remove_file(const char *filename);
can5_err_t can5_storage_mkdir_p(const char *dirname);
can5_err_t can5_storage_register_wearlevel_tag(const char *tag);

can5_err_t can5_storage_push_ram(const char *tag, const uint8_t *data, const size_t data_len);
can5_err_t can5_storage_pop_ram(const char *tag, uint8_t *data, size_t *data_len);
can5_err_t can5_storage_peek_ram(const char *tag, uint8_t *data, size_t *data_len);
can5_err_t can5_storage_write_ram(const char *tag, const char *key, const uint8_t *data, const size_t data_len);
can5_err_t can5_storage_read_ram(const char *tag, const char *key, uint8_t *data, size_t *data_len);
can5_err_t can5_storage_remove_ram(const char *tag, const char *key);
can5_err_t can5_storage_remove_tag_ram(const char *tag);

can5_err_t can5_storage_write_int_nvs(const char *tag, const char *key, int32_t data);
can5_err_t can5_storage_read_int_nvs(const char *tag, const char *key, int32_t *data);
can5_err_t can5_storage_remove_nvs(const char *tag, const char *key);
can5_err_t can5_storage_remove_tag_nvs(const char *tag);

int32_t can5_storage_status_get();
const char* can5_storage_evt_getstr(int32_t evt);
#endif //__CAN5_STORAGE_H__
