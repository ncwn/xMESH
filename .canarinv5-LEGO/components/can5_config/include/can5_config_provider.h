/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   18/01/22
*
* File:  can5_config_provider.h
* Descr: Use this to get configuration
*******************************************************************************/

#ifndef CAN5_APP_CAN5_CONFIG_PROVIDER_H
#define CAN5_APP_CAN5_CONFIG_PROVIDER_H

#include <stddef.h>
#include "can5_error.h"
#include "can5_config.h"
can5_err_t config_fs_init();

can5_err_t config_write(can5_cfg_type_t type, const uint8_t *data, size_t len);
can5_err_t config_read(can5_cfg_type_t type, uint8_t **data, size_t *len);

can5_err_t config_write_bool(can5_cfg_type_t type, bool data);
can5_err_t config_read_bool(can5_cfg_type_t type, bool *data);

can5_err_t config_write_int(can5_cfg_type_t type, int64_t data);
can5_err_t config_read_int(can5_cfg_type_t type, int64_t *data);

can5_err_t config_write_double(can5_cfg_type_t type, double data);
can5_err_t config_read_double(can5_cfg_type_t type, double *data);

can5_err_t config_remove(can5_cfg_type_t type);
can5_err_t config_remove_tag(const char *tag);
can5_err_t config_remove_file(const char *filename);

can5_err_t update_counter(const char *key, bool increment);
can5_err_t get_counter(const char *key, int64_t *val);

can5_err_t commit_config_to_disk();

#endif //CAN5_APP_CAN5_CONFIG_PROVIDER_H
