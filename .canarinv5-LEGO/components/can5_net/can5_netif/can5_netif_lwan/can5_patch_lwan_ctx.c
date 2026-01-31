/**************************************************
 * Author: rmukhia
 * Creation Date: 1/8/22
 * Description: 
 **************************************************/

#include <string.h>
#include <endian.h>
#include "esp_log.h"
#include "can5_patch_lwan_ctx.h"
#include "can5_utils.h"

const static char *TAG = "PATCH_LWAN_CTX";


/*
 * CTX0
 * DataRate Size 1, offset 133 default 5<\r><\n>
 * UplinkDwellTime Size 1, offset 184 default 56<\r><\n>
 * DownlinkDwellTime Size 1, offset 185 default 57<\r><\n>
*/

void can5_patch_lwan_ctx_dwell_time(char *in_out_ctx0, bool uplink_dwell_time, bool downlink_dwell_time)
{

    ESP_LOGI_V(TAG, "patching dwell time");
    char *ptr;

    // patch uplink dwell time
    ptr = &in_out_ctx0[strlen("0:xxx:") + (184 * 2)];

    if (uplink_dwell_time) {
        ptr[0] = '0';
        ptr[1] = '1';
    }
    else {
        ptr[0] = '0';
        ptr[1] = '0';
    }

    // patch downlink dwell time
    ptr = &in_out_ctx0[strlen("0:xxx:") + (185 * 2)];

    if (downlink_dwell_time) {
        ptr[0] = '0';
        ptr[1] = '1';
    }
    else {
        ptr[0] = '0';
        ptr[1] = '0';
    }
}

void can5_patch_lwan_ctx_framecounters(char *in_out_ctx2, bool reset_fcnt_up, bool reset_fcnt_down)
{
    ESP_LOGI(TAG, "patching framecounters");

    char *ptr;

    ptr = &in_out_ctx2[strlen("2:xx:") + (12 * 2)];

    if (reset_fcnt_up) {
        memset(ptr, '0', 4 * 2);    // 4 bytes, 8 hex
        ptr[1] = '1';               // reset to 1
    }

    ptr = &in_out_ctx2[strlen("2:xx:") + (24 * 2)];

    if (reset_fcnt_down) {
        memset(ptr, '0', 4 * 2);    // 4 bytes, 8 hex
        ptr[1] = '1';               // reset to 1
    }
}

void can5_patch_lwan_ctx_get_framecounters(const char *in_out_ctx2, uint32_t *fcnt_up, uint32_t *fcnt_down)
{
    const char *ptr;
    uint32_t b_data;

    ptr = &in_out_ctx2[strlen("2:xx:") + (12 * 2)];

    can5_hex_to_bin(ptr, &b_data, 4);

    *fcnt_up = b_data;

    ptr = &in_out_ctx2[strlen("2:xx:") + (24 * 2)];

    can5_hex_to_bin(ptr, &b_data, 4);
    *fcnt_down = b_data;
}
