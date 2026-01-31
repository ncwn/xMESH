/**************************************************
 * Author: rmukhia
 * Creation Date: 27/6/22
 * Description: 
 **************************************************/
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "can5.h"
#include "sdkconfig.h"
#ifdef CONFIG_CAN5_HAL_BUILD_TESTS
#include "can5_hal_test.h"
#endif
#ifdef CONFIG_CAN5_RTC_BUILD_TESTS
#include "can5_rtc_test.h"
#include "can5_rtc.h"
#endif
#ifdef CONFIG_CAN5_SENSOR_BUILD_TESTS
#include "can5_sensor_test.h"
#endif
#ifdef CONFIG_CAN5_NET_BUILD_TESTS
#include "can5_netif_lwan2.h"
#include "can5_netif_wifi.h"
#include "can5_netif_test.h"
#endif
#ifdef CONFIG_CAN5_CONFIG_BUILD_TEST
#include "can5_config_test.h"
#endif

can5_err_t can5_tests()
{

#ifdef CONFIG_CAN5_HAL_BUILD_TESTS
    CAN5_ERR_CHECK_NO_ABORT(hal_test_adc());
    CAN5_ERR_CHECK_NO_ABORT(hal_test_i2c());
    CAN5_ERR_CHECK_NO_ABORT(hal_test_serial());
#endif

    vTaskDelay(pdMS_TO_TICKS(2000));

#ifdef CONFIG_CAN5_RTC_BUILD_TESTS
    CAN5_ERR_CHECK_NO_ABORT(rtc_test_start());
    CAN5_ERR_CHECK_NO_ABORT(rtc.print_rtc_time());
#endif

    vTaskDelay(pdMS_TO_TICKS(2000));

#ifdef CONFIG_CAN5_SENSOR_BUILD_TESTS
    CAN5_ERR_CHECK_NO_ABORT(sensordriv_test_co());
    CAN5_ERR_CHECK_NO_ABORT(sensordriv_test_bme280());
    CAN5_ERR_CHECK_NO_ABORT(sensordriv_test_ublox_neo_m8m());
    CAN5_ERR_CHECK_NO_ABORT(sensordriv_test_mhz16());
    CAN5_ERR_CHECK_NO_ABORT(sensordriv_test_ublox_neo_m8m());
    CAN5_ERR_CHECK_NO_ABORT(sensordriv_test_mhz16());
#endif

    vTaskDelay(pdMS_TO_TICKS(2000));

#ifdef CONFIG_CAN5_NET_BUILD_TESTS
    CAN5_ERR_CHECK_NO_ABORT(wifi_test_full(&netif_wifi));
    static can5_netif_wifi_driverctl_params_t params;
    params.scan_list_size = 16;
    netif_wifi.ops.driverctl(CAN5_DRIVERCTL_NETIF_WIFI_SCAN, &params, NULL);
    CAN5_ERR_CHECK_NO_ABORT(netif_test_full(&netif_lwan2));
#endif

    vTaskDelay(pdMS_TO_TICKS(2000));

#ifdef CONFIG_CAN5_CONFIG_BUILD_TEST
    CAN5_ERR_CHECK_NO_ABORT(config_test_start());
#endif

    return CAN5_SUCCESS;
}