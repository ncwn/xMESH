#ifndef __CAN5_STORAGE_STUB_H__
#define __CAN5_STORAGE_STUB_H__

#include "can5_storagedriv.h"

can5_err_t can5_nvs_init();
can5_err_t can5_nvs_remove_row(const char *tag, const char *key);
can5_err_t can5_nvs_remove_tag(const char *tag);
can5_err_t can5_nvs_uninit();
can5_err_t can5_nvs_write_int(const char *tag, const char *key, int32_t data);
can5_err_t can5_nvs_read_int(const char *tag, const char *key, int32_t *data);


#endif //__CAN5_STORAGE_STUB_H__
