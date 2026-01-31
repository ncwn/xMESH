#include <string.h>
#include <can5_events.h>
#include <can5_config_provider.h>
#include "can5_netif_test.h"
#include "can5_netmng.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "can5_config_types.h"
#include "can5_storagemng.h"

#define TAG "netif_test"

#define CHECK_RESULT(r) do { \
int ret;  \
ret = r;  \
if (ret == CAN5_SUCCESS) ESP_LOGI(TAG, "OK!");  else CAN5_ERR_CHECK_NO_ABORT(ret);   \
} while (0)



static int32_t event = 0;



static void _to_cb(xTimerHandle ev) {
    bool* toflag;
    toflag = pvTimerGetTimerID(ev);
    *toflag = true;
}


static can5_err_t _wait_for_bool(bool expected, bool (*function)(void), uint16_t timeout) {
    xTimerHandle _to_hdl = NULL; 
    volatile bool to_flag = false; 

    _to_hdl = xTimerCreate("_timeout", pdMS_TO_TICKS(timeout), pdFALSE, (void*)&to_flag, _to_cb);
    
    VERIFY_NOT_NULL(_to_hdl);
    VERIFY_NOT_NULL(function);
    VERIFY_pdPASS(xTimerStart(_to_hdl, 5));
    
    char chr;
    
    uint8_t p = 0;
    do {
        vTaskDelay(10);
    } while (function() != expected);

    if (to_flag) {
        return CAN5_NET_ERR_NIC_TIMEOUT;
    }   

    if (xTimerIsTimerActive(_to_hdl) != pdFALSE) {
        VERIFY_pdPASS(xTimerStop(_to_hdl, portMAX_DELAY));  // avoid firing after the function exited (and variables deallocated)
    }

    return CAN5_SUCCESS;
}

static void __conn_cb (const can5_net_connect_evt_t* evt) {
    VERIFY_NOT_NULL_VOID(evt);
    ESP_LOGI(TAG, "Connection event: %s", connevt_getstr(evt->type));
}

static void __rx_cb (const void* buffer, size_t len) {
    VERIFY_NOT_NULL_VOID(buffer);
    ESP_LOGI(TAG, "Receive Callback: %dbytes", len);
    ESP_LOG_BUFFER_HEXDUMP(TAG, buffer, len, ESP_LOG_INFO);
    event = CAN5_NET_EVT_RECVD;
    free(buffer);
}

static can5_netif_ops_t* pops;

can5_err_t wifi_test_full(can5_netif_t* netif) {
    can5_err_t r;

    VERIFY_NOT_NULL(netif);

    ESP_LOGI(TAG, "###### Starting Netif Wifi Driver Test");

    VERIFY_SUCCESS(storage_manager.set_active(CAN5_STORAGE_TYPE_SDSPI));

    static char buffer[10];
    static size_t buffsz;

    if (netif->details.type != CAN5_NET_TYPE_WIFI) {
        ESP_LOGE(TAG, "Not a WIFI driver");
        return CAN5_NET_ERR_BASE;
    }

    pops = &netif->ops;
    ESP_LOGI(TAG, "Detect on NETPORT_0");
    CHECK_RESULT(pops->detect(NETPORT_0));

    ESP_LOGI(TAG, "Writing Wifi Config");
    char *ssid = "bitwise";
    char *pass = "1234abcd@";

    CHECK_RESULT(config_write(CFG_WIFI_STA_SSID, (uint8_t *) ssid, strlen(ssid)));
    CHECK_RESULT(config_write(CFG_WIFI_STA_PASS, (uint8_t *) pass, strlen(pass)));
    ESP_LOGI(TAG, "Init on NETPORT_0");
    can5_err_t _ret = pops->init(NETPORT_0);
    CHECK_RESULT(_ret);
    VERIFY_SUCCESS(_ret);

    char *ip = "10.42.0.1";
    int port = 4321;
    CHECK_RESULT(config_write(CFG_HAZEMON_IP, (uint8_t *) ip, strlen(ip)));
    CHECK_RESULT(config_write(CFG_HAZEMON_PORT, (uint8_t *) &port, sizeof(port)));
    ESP_LOGI(TAG, "Connect wait");
    CHECK_RESULT(pops->connect(__conn_cb, true));

    ESP_LOGI(TAG, "Connect do not wait");
    CHECK_RESULT(pops->connect(__conn_cb, false));

    ESP_LOGI(TAG, "Wait for connection");
    CHECK_RESULT(_wait_for_bool(true, pops->is_connected, 10000));


    ESP_LOGI(TAG, "Send Test: ");
    CHECK_RESULT(pops->send("netif test send 1\n", 19));

    ESP_LOGI(TAG, "Blocking receive");
    buffsz = sizeof(buffer);
    CHECK_RESULT(pops->recv(buffer, &buffsz, 1000));
    ESP_LOGI(TAG, "recv: ");
    ESP_LOG_BUFFER_HEXDUMP(TAG, buffer, buffsz, ESP_LOG_INFO);

    ESP_LOGI(TAG, "Register rx callback");
    //CHECK_RESULT(pops->register_recv_cb(__rx_cb));

    ESP_LOGI(TAG, "RSSI: ");
    // ESP_LOGI(TAG, "rssi %d", pops->rssi_get());

    ESP_LOGI(TAG, "Send Test: ");
    CHECK_RESULT(pops->send("netif test send 2\n", 19));

    ESP_LOGI(TAG, "Dump statuses: ");

    for (int s=0; s< 10; s++) {
        ESP_LOGI(TAG, "Print Status: %d: %s", s, pops->status_getstr(s));
    }

    ESP_LOGI(TAG, "Waiting for receive...");

    while (event != CAN5_NET_EVT_RECVD) {
        CHECK_RESULT(pops->run());
        vTaskDelay(100);
    }

    ESP_LOGI(TAG, "UNINIT ");
    CHECK_RESULT(pops->uninit());

    ESP_LOGI(TAG, "Test complete!");
    return CAN5_SUCCESS;
}

can5_err_t netif_test_full(can5_netif_t* netif) {
    can5_err_t r;
    
    VERIFY_NOT_NULL(netif);

    ESP_LOGI(TAG, "###### Starting Netif Driver Test");

    static char buffer[10];
    static size_t buffsz;
    can5_port_idx_t port = 0;

    if (netif->details.type == CAN5_NET_TYPE_WIFI) {
        port = NETPORT_0;
    } else {
        port = NETPORT_1;
    }

    pops = &netif->ops;
    ESP_LOGI(TAG, "Detect on NETPORT_%d", port);
    CHECK_RESULT(pops->detect(port));

    ESP_LOGI(TAG, "Register rx callback");
    CHECK_RESULT(pops->register_recv_cb(__rx_cb));

    char *appeui = "12:12:12:12:12:12:12:12";
    char *adaptive_data_rate = "0";
    char *data_rate = "5";

    CHECK_RESULT(config_write(CFG_LWAN_APPEUI,
                              (uint8_t *) appeui, strlen(appeui)));
    CHECK_RESULT(config_write(CFG_LWAN_ADAPTIVE_DATA_RATE,
                              (uint8_t *) adaptive_data_rate, strlen(adaptive_data_rate)));
    CHECK_RESULT(config_write(CFG_LWAN_DATA_RATE,
                              (uint8_t *) data_rate, strlen(data_rate)));
    ESP_LOGI(TAG, "Init on NETPORT_%d", port);
    CHECK_RESULT(pops->init(port));
    while (pops->status_get() < 2) {
        pops->run();
        vTaskDelay(pdMS_TO_TICKS(100));

    }

    //ESP_LOGI(TAG, "Connect wait");
    //CHECK_RESULT(pops->connect(__conn_cb, true));
    //pops->run();

    ESP_LOGI(TAG, "Connected");

    ESP_LOGI(TAG, "Connect do not wait");
    CHECK_RESULT(pops->connect(__conn_cb, false));
    while (!pops->is_connected()) {
        pops->run();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    CHECK_RESULT(pops->register_recv_cb(__rx_cb));
    ESP_LOGI(TAG, "Send Test: ");
    CHECK_RESULT(pops->send("helloworldhelloworldhelloworld", 30));
    while(1) {
        pops->run();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }


#if 0



    ESP_LOGI(TAG, "Waiting for receive...");


    ESP_LOGI(TAG, "Blocking receive");
    buffsz = sizeof(buffer);
    CHECK_RESULT(pops->recv(buffer, &buffsz, 1000));
    ESP_LOGD(TAG, "recv: ");
    ESP_LOG_BUFFER_HEXDUMP(TAG, buffer, buffsz, ESP_LOG_DEBUG);


    ESP_LOGI(TAG, "RSSI: ");
    ESP_LOGI(TAG, "%d", pops->rssi_get());

    ESP_LOGI(TAG, "Send Test: ");
    CHECK_RESULT(pops->send("netif test send\n", 17));

    ESP_LOGI(TAG, "Dump statuses: ");
    for (int s=0; s< 10; s++) { 
        ESP_LOGI(TAG, "Print Status: %d: %s", s, pops->status_getstr(s));
    }


    ESP_LOGI(TAG, "Waiting for receive...");

    while (event != CAN5_NET_EVT_RECVD) {
        CHECK_RESULT(pops->run());
        vTaskDelay(100);
    }

#endif

    ESP_LOGI(TAG, "UNINIT ");
    //CHECK_RESULT(pops->uninit());


    ESP_LOGI(TAG, "Test complete!");
    return CAN5_SUCCESS;
}
