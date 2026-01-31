#ifndef __CAN5_STORAGE_COMB_H__
#define __CAN5_STORAGE_COMB_H__

#include "can5_storagedriv.h"
#include "can5_pins.h"

#ifdef CAN5_LINUX_HOST_TEST
#define CAN5_STORAGE_FSFAT_MOUNT_DIR     ""
#define CAN5_STORAGE_FSFAT_DICT_DIR      "data/config/"
#define CAN5_STORAGE_FSFAT_STACK_DIR     "data/data/"
#define CAN5_STORAGE_FSFAT_LOG_DIR       "data/log/"
#else
#define CAN5_STORAGE_FSFAT_MOUNT_DIR     CAN5_FATFS_MOUNT_DIR
#define CAN5_STORAGE_FSFAT_DICT_DIR      CAN5_STORAGE_FSFAT_MOUNT_DIR "/config/"
#define CAN5_STORAGE_FSFAT_STACK_DIR     CAN5_STORAGE_FSFAT_MOUNT_DIR "/data/"
#define CAN5_STORAGE_FSFAT_LOG_DIR       CAN5_STORAGE_FSFAT_MOUNT_DIR "/log/"
#endif

#define CAN5_LIFO_FILENAME_MAX  32


can5_err_t can5_fatfs_init();
can5_err_t can5_fsfat_uninit();
can5_err_t can5_fsfat_lifo_remove_old_data(const char *tag);
can5_err_t can5_fsfat_remove_tag(const char *tag);
can5_err_t can5_fsfat_make_dir_p(const char *path);
can5_err_t can5_fsfat_write_file(const char *filename, const uint8_t *data, size_t len);
can5_err_t can5_fsfat_read_file_len(const char *filename, size_t *len);
can5_err_t can5_fsfat_read_file(const char *filename, uint8_t *data, size_t len);
can5_err_t can5_fsfat_remove_file(const char *filename);

can5_err_t can5_fsfat_di_write(const char *tag, const char *key, const uint8_t *buf, size_t buf_len);
can5_err_t can5_fsfat_di_read(const char *tag, const char *key, uint8_t *buf, size_t *buf_len);
can5_err_t can5_fsfat_di_delete(const char *tag, const char *key);
can5_err_t can5_fsfat_di_remove_tag(const char *tag);
can5_err_t can5_fsfat_di_commit();
can5_err_t can5_fsfat_di_register_wearlevel_tag(const char *tag);
can5_err_t can5_fsfat_di_unregister_wearlevel();

can5_err_t can5_fsfat_lifo_push(const char *tag, const uint8_t *buf, size_t buf_len);
can5_err_t can5_fsfat_lifo_pop(const char *tag, uint8_t *buf, size_t *buf_len);
can5_err_t can5_fsfat_lifo_peek(const char *tag, uint8_t *buf, size_t *buf_len);
can5_err_t can5_fsfat_lifo_remove_tag(const char *tag);

#endif //__CAN5_STORAGE_COMB_H__
