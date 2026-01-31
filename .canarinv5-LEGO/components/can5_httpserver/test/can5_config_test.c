#include <string.h>
#include "esp_log.h"
#include "can5_config.h"
#include "can5_config_test.h"
#include "can5_storagemng.h"

static const char *TAG = "STORAGE_TEST";

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



can5_err_t config_test_start() {
    VERIFY_NOT_NULL(&config_manager);

    //can5_config.start();

    return CAN5_SUCCESS;
}
