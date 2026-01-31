#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "can5_storagedriv.h"

#define TAG "storage_test"

#define CHECK_RESULT(r) do { \
int ret;  \
ret = r;  \
if (ret == CAN5_SUCCESS) ESP_LOGI(TAG, "OK!");  else CAN5_ERR_CHECK_NO_ABORT(ret);   \
} while (0)


#define CHECK_RESULT_FAIL(r) do { \
int ret;  \
ret = r;  \
if (ret != CAN5_SUCCESS) ESP_LOGI(TAG, "OK Failed %d!", ret);  \
} while (0)


static can5_storagedriv_ops_t * pops;

can5_err_t storage_driver_test_full(can5_storagedriv_t* storage) {

    uint8_t data[10];
    uint8_t edata[10]  = {0};
    size_t data_len;
    static char dynamic_data[255];
    
    VERIFY_NOT_NULL(storage);

    pops = &storage->ops;

    ESP_LOGI(TAG, "###### Starting Storage SD SPI Driver Test");
    CHECK_RESULT(pops->init());
    strcpy((char *)data, "hello1");
    CHECK_RESULT(pops->write("pm321", "11", data, strlen((char *)data)));
    CHECK_RESULT(pops->read("pm321", "11", edata, &data_len));
    ESP_LOGI(TAG, "Read Data %s %u!", edata, data_len);
    CHECK_RESULT(pops->remove("pm321", "11"));
    CHECK_RESULT_FAIL(pops->read("pm321", "11", edata, &data_len));
    strcpy((char *)data, "hello2");
    CHECK_RESULT(pops->write("pm321", "11", data, strlen((char *)data)));
    CHECK_RESULT(pops->read("pm321", "11", edata, &data_len));
    ESP_LOGI(TAG, "Read Data %s %u!", edata, data_len);
    CHECK_RESULT(pops->remove_tag("pm321"));

    int test_iter = 20;
    for (int i = 0; i < test_iter; i++) {
        ESP_LOGI(TAG, "Pushing data %d", i);
        sprintf(dynamic_data, "data-%d", i);
        CHECK_RESULT(pops->push("ddata", (uint8_t *)dynamic_data, strlen(dynamic_data)));
    }

    for (int i = 0; i < test_iter; i++) {
        memset(dynamic_data, 0 , 255);
        CHECK_RESULT(pops->pop("ddata", (uint8_t *)dynamic_data, &data_len));
        ESP_LOGI(TAG, "Popping data %d : %s (%u)", i, dynamic_data, data_len);
    }
    CHECK_RESULT(pops->remove_tag("ddata"));
    CHECK_RESULT(pops->uninit());
    ESP_LOGI(TAG, "Test complete!");
    return CAN5_SUCCESS;
}
