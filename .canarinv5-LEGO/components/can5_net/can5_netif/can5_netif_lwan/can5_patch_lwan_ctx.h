/**************************************************
 * Author: rmukhia
 * Creation Date: 1/8/22
 * Description: 
 **************************************************/

#ifndef CANARINV5_LEGO_CAN5_PATCH_LWAN_CTX_H
#define CANARINV5_LEGO_CAN5_PATCH_LWAN_CTX_H

#include <stdint.h>
#include <stdbool.h>

void can5_patch_lwan_ctx_dwell_time(char *in_out_ctx0, bool uplink_dwell_time, bool downlink_dwell_time);

void can5_patch_lwan_ctx_framecounters(char *in_out_ctx2, bool reset_fcnt_up, bool reset_fcnt_down);

void can5_patch_lwan_ctx_get_framecounters(const char *in_out_ctx2, uint32_t *fcnt_up, uint32_t *fcnt_down);
#endif //CANARINV5_LEGO_CAN5_PATCH_LWAN_CTX_H
