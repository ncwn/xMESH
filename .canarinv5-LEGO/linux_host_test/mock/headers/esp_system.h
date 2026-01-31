/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   02/02/22
*
* File:  esp_system.h
* Descr:
*******************************************************************************/

#ifndef TEST_APP_ESP_SYSTEM_H
#define TEST_APP_ESP_SYSTEM_H
#include <stdint.h>
#include <stdlib.h>
#include <memory.h>

static uint8_t mock_mac_address[6] = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0
};

static int esp_get_free_heap_size() { return 500; }

static int esp_efuse_mac_get_default(uint8_t *mac) {
    memcpy(mac, mock_mac_address, sizeof mock_mac_address);
    return 0;
}

#define esp_system_abort(x)      exit(0);

#endif //TEST_APP_ESP_SYSTEM_H
