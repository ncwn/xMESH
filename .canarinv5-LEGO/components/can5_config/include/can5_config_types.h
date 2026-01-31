//
// Created by rmukhia on 29/08/21.
//

#ifndef __CAN5_CFG_H__
#define __CAN5_CFG_H__
#include "can5_config.h"
#include "can5_storagedriv.h"

#define CAN5_CFG_PORT_MIN CFG_DATA_CYCLE_SEC
#define CAN5_CFG_PORT_MAX CFG_UART_7

typedef enum can5_cfg_storage_type_e {
    CAN5_CFG_STORAGE_TYPE_TAG_FS = 0,
    CAN5_CFG_STORAGE_TYPE_RAW_FS,
    CAN5_CFG_STORAGE_TYPE_RAM,
    CAN5_CFG_STORAGE_TYPE_NVS,
    CAN5_CFG_STORAGE_TYPE_COUNT,
} can5_cfg_storage_type_t;

typedef struct can5_config_meta_s {
    const can5_cfg_type_t type;
    const char *tag;
    const can5_cfg_storage_type_t storage_type;
    const char *default_val;
    const size_t max_len;
} can5_cfg_meta_t;

typedef struct can5_config_meta_tags_s {
    const char *tag;
    const bool wear_leveling;
} can5_cfg_meta_tags_t;

const can5_cfg_meta_t *can5_config_get_all_meta_elems (size_t *len);

const can5_cfg_meta_tags_t *can5_get_cfg_tag_list(size_t *len);

const char **can5_get_cfg_raw_files_list(size_t *len);

const can5_cfg_meta_t *can5_config_get_meta(can5_cfg_type_t type);

const char* can5_config_getstr(can5_cfg_type_t type);

const char* can5_config_get_key_str(const can5_cfg_type_t type);

const char* can5_config_storage_type_getstr(const can5_cfg_storage_type_t type);
#endif // __CAN5_CFG_H__
