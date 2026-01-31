#include <can5_events.h>
#include "can5_hal.h"
#include "can5_wiring.h"
#include "can5_utils.h"
#include "esp_log.h"
#include "esp_event.h"
#include "can5_hal_test.h"
#include "freertos/FreeRTOS.h"

#define TAG "hal_test"

#define CHECK_RESULT(r) do { \
int ret;  \
ret = r;  \
if (ret == CAN5_SUCCESS) ESP_LOGI(TAG, "OK!");  else CAN5_ERR_CHECK_NO_ABORT(ret);   \
} while (0)



static int32_t event = 0;

static void _hal_evt_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data) {
    
    ESP_LOGD(TAG, "[%d] HAL_EVT: %d - %s", event++, event_id, CAN5_MODULE_EVT_STR(hal)(event_id));
    ESP_LOGD(TAG, "event data: %s", (char *)event_data);
    // switch (event_id) {

    //     case CAN5_HAL_EVT_NETPORT_RECVD: {

    //     } break;
        
    //     case CAN5_HAL_EVT_UPORT_RECVD: {

    //     } break;

    //     case CAN5_HAL_EVT_WAKE_INT_FALL: {

    //     } break;
        
    //     default:
    //         break;
    //     }
    // ESP_LOGD(TAG, "");
}

can5_err_t hal_test_serial() {
    
    can5_err_t r; 
    
    static size_t buffsz;

    esp_event_handler_register(CAN5_EVT_HAL, ESP_EVENT_ANY_ID, _hal_evt_handler, NULL);

    char buffer[] = "hal test  p.XX\n";
    buffsz = sizeof(buffer);
    
    ESP_LOGI(TAG, "Init:");
    CHECK_RESULT(CAN5_MODULE_INIT(hal)());

    can5_port_idx_t p;
    
    for (p=0; p< CAN5_PORT_COUNT; p++) { 
        snprintf(buffer, buffsz, "hal test  p.%02d\n", p);
        ESP_LOGI(TAG, "serial_send on port %d [%s]:", p, can5_hal_port_getstr(p));
        CHECK_RESULT(hal.serial_send(buffer, buffsz, p, 10));

        ESP_LOGI(TAG, "serial_recv on port %d [%s]:", p, can5_hal_port_getstr(p));

        CLEAR_ARRAY(buffer);
        r = hal.serial_recv(buffer, &buffsz, p, 100);
        if (r == CAN5_SUCCESS) {
            ESP_LOGI(TAG, "Received %d bytes", buffsz);
            ESP_LOG_BUFFER_HEXDUMP(TAG, buffer, buffsz, ESP_LOG_DEBUG);
        } else {
            CHECK_RESULT(r);
        }

    }



    int32_t s = CAN5_MODULE_STAT_GET(hal)();
    ESP_LOGI(TAG, "Get Status: %d: %s", s, CAN5_MODULE_STAT_STR(hal)(s));
    for (s=0; s< 10; s++) { 
        ESP_LOGI(TAG, "Print Status: %d: %s", s, CAN5_MODULE_STAT_STR(hal)(s));
    }


    do { 
        CLEAR_ARRAY(buffer);
        buffsz = 1;
        r = hal.serial_recv(buffer, &buffsz, NETPORT_1, 0xFFFF);
        ESP_LOGI(TAG, "%s", buffer);
    } while (buffer[0] != '\n');


    ESP_LOGI(TAG, "Waiting for interrupt:");
    //while (cont){
    while(!event) {
        vTaskDelay(1);
        // vApplicationSleep(pdMS_TO_TICKS(10));
    }
    ESP_LOGI(TAG, "Evt wait unlocked");
    event = 0;
    //}
    

    ESP_LOGI(TAG, "Test complete!");
    return CAN5_SUCCESS;
}


can5_err_t hal_test_port_enable() {
    
    bool en = false; 
    while (1) {
        en = !en;
        for (int p = UPORT_0; p< UPORT_COUNT; p++ ) {
            ESP_LOGD(TAG, "%s port %d", en?"Enabling":"Disabling", p);
            CHECK_RESULT(hal.enable(en, p));
            vTaskDelay(pdMS_TO_TICKS(10));
        }

    }
    return CAN5_SUCCESS;
}

static volatile struct  serial_port_switch_ctrl_s {
    volatile bool toggle_en;
    volatile can5_port_idx_t port;
    volatile bool net_data_present;
} __serial_port_switch_ctrl = {false, UPORT_0, false};




static void _port_switch_hal_evt_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {

    
    //VERIFY_NOT_NULL_VOID(arg);

    
    switch (event_id) {
        case CAN5_HAL_EVT_WAKE_INT_FALL:
            __serial_port_switch_ctrl.port = (__serial_port_switch_ctrl.port == UPORT_0) ? UPORT_1 : UPORT_0;
            ESP_LOGD(TAG, "WAKE Interrupt, select port: %s", can5_hal_port_getstr(__serial_port_switch_ctrl.port));
            break;
        
        case CAN5_HAL_EVT_USRBTN_INT_RISE :
            __serial_port_switch_ctrl.toggle_en = true ;
            ESP_LOGD(TAG, "USER BUTTON Interrupt");
            break;

        case CAN5_HAL_EVT_NET_UART_PATTERN :
            ESP_LOGD(TAG, "NETPORT Pattern Detect %s", can5_hal_port_getstr((can5_port_idx_t)event_data));
            __serial_port_switch_ctrl.net_data_present = true ;
            break;
        
        default:
            break;
    }
    
}

can5_err_t hal_test_adc() {

    while (1) {
        uint16_t sample = 0;
        hal.analog_read(&sample, ADPORT_0);

        ESP_LOGI(TAG, "ADC: %d\n", sample);
        vTaskDelay(500);
    }

}


can5_err_t hal_test_i2c() {
    ESP_LOGI(TAG, " ---> Starting Hal I2C Test uSING MPU6050 <----- ");
    uint8_t buffer[4];
    // for (uint8_t reg = 0x0D; reg<=0x75; reg++) {
    //     CAN5_ERR_CHECK_NO_ABORT(hal.i2c_read_reg(0b1101000, reg, buffer, 1, I2C_MASTER_LAST_NACK));
    //     ESP_LOGI(TAG, "reg 0x%02X, val:  0x%02X", reg, buffer[0]);
    //     vTaskDelay(200/portTICK_RATE_MS);
    //     // hal.i2c_read_reg(0b1101001, 0x75, buffer, 1, I2C_MASTER_LAST_NACK);
    //     // ESP_LOGI(TAG, "dev 0b1101001 reg 0x75, val:  0x%02X", buffer[0]);
    //     // vTaskDelay(500/portTICK_RATE_MS);

    // }
    uint8_t reg = 0x6B;
    buffer[0] = 0;
    CAN5_ERR_CHECK_NO_ABORT(hal.i2c_write_reg(0b1101000, reg, buffer, 1, true));
    CAN5_ERR_CHECK_NO_ABORT(hal.i2c_read_reg(0b1101000, reg, &buffer[1], 1, I2C_MASTER_LAST_NACK));
    ESP_LOGI(TAG, "reg 0x%02X, read:  0x%02X", reg, buffer[1]);
    reg = 0x1B;
    buffer[0] = 0x10;
    CAN5_ERR_CHECK_NO_ABORT(hal.i2c_write_reg(0b1101000, reg, buffer, 1, true));
    CAN5_ERR_CHECK_NO_ABORT(hal.i2c_read_reg(0b1101000, reg, &buffer[1], 1, I2C_MASTER_LAST_NACK));
    ESP_LOGI(TAG, "reg 0x%02X, read:  0x%02X", reg, buffer[1]);
    reg = 0x1C;
    buffer[0] = 0x10;
    CAN5_ERR_CHECK_NO_ABORT(hal.i2c_write_reg(0b1101000, reg, buffer, 1, true));
    CAN5_ERR_CHECK_NO_ABORT(hal.i2c_read_reg(0b1101000, reg, &buffer[1], 1, I2C_MASTER_LAST_NACK));
    ESP_LOGI(TAG, "reg 0x%02X, read:  0x%02X", reg, buffer[1]);
    while(1) {
        uint8_t reg = 0x3B;
        CAN5_ERR_CHECK_NO_ABORT(hal.i2c_read_reg(0b1101000, reg, buffer, 2, I2C_MASTER_LAST_NACK));
        int16_t val = __bswap16(*(int16_t*)buffer);
        ESP_LOGI(TAG, "reg 0x%02X, read:  0x%02X%02x, val: %d", reg, buffer[0], buffer[1], val);
        vTaskDelay(200/portTICK_RATE_MS);
    }
    return CAN5_SUCCESS;
}


can5_err_t hal_test_serial_port_switch() {
    uint8_t buffer[100];
    size_t  len;

    ESP_LOGI(TAG, " ---> Starting Hal Serial Port Test <----- ");
    VERIFY_SUCCESS(esp_event_handler_register(CAN5_EVT_HAL, ESP_EVENT_ANY_ID, _port_switch_hal_evt_handler, NULL));
    ESP_LOGI(TAG, "Install  pattern detect interrupts ");
    CAN5_ERR_CHECK_NO_ABORT(hal.serial_char_detect_install('A', UPORT_1));
    CAN5_ERR_CHECK_NO_ABORT(hal.serial_char_detect_install('B', NETPORT_1));
    while (1) {
        if (__serial_port_switch_ctrl.toggle_en) {
            __serial_port_switch_ctrl.toggle_en = false;
            CAN5_ERR_CHECK_NO_ABORT(hal.enable(!hal.is_enabled(__serial_port_switch_ctrl.port), __serial_port_switch_ctrl.port));  // toggle enable port
        }
        static uint8_t get_co2_cmd[] = {0XFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};

        hal.serial_send(get_co2_cmd, sizeof(get_co2_cmd), __serial_port_switch_ctrl.port, 0);
        len = 1;
        vTaskDelay(50);
        hal.serial_recv(buffer, &len,__serial_port_switch_ctrl.port,10);
        if (len) {
            uint32_t i = len;
            while (len) {
                len = 100-i;
                hal.serial_recv(&buffer[i], &len, __serial_port_switch_ctrl.port,10);
                i+=len;
            }
            ESP_LOGD(TAG, "RX[%s]: %d bytes", can5_hal_port_getstr(__serial_port_switch_ctrl.port), i);
            ESP_LOG_BUFFER_HEX(TAG, buffer, i);
            if (buffer[0] == 'Z') {
                ESP_LOGI(TAG, "REMOVE  pattern detect interrupt on port UPORT_0");
                hal.serial_char_detect_remove(UPORT_1);
            }
            if (buffer[0] == 'Y') {
                ESP_LOGI(TAG, "REMOVE  pattern detect interrupt on port NETPORT_1");
                hal.serial_char_detect_remove(UPORT_0);
            }
        }

        if ( __serial_port_switch_ctrl.net_data_present) {

            size_t len = 100;
            static char b[100];
            hal.serial_recv(b, &len, NETPORT_1, 10);
            if (len) {
                ESP_LOGI(TAG, "RX[%s]: %d bytes", can5_hal_port_getstr(NETPORT_1), len);
                ESP_LOG_BUFFER_HEX(TAG, b, len);

            }
        }
    }
}

can5_err_t hal_test_port_reenable() {
    while (1) {

        //ESP_LOGD(TAG, "%s port %d", 1?"Enabling":"Disabling", UPORT_0);
        CHECK_RESULT(hal.enable(1, UPORT_0));
        vTaskDelay(pdMS_TO_TICKS(1));
        //vTaskDelay(pdMS_TO_TICKS(10));
        //ESP_LOGD(TAG, "%s port %d", 0?"Enabling":"Disabling", UPORT_1);
        CHECK_RESULT(hal.enable(0, UPORT_1));
        vTaskDelay(pdMS_TO_TICKS(1));
        //vTaskDelay(pdMS_TO_TICKS(10));
        //ESP_LOGD(TAG, "%s port %d", 1?"Enabling":"Disabling", UPORT_1);
        CHECK_RESULT(hal.enable(1, UPORT_4));
        vTaskDelay(pdMS_TO_TICKS(1));
        CHECK_RESULT(hal.enable(0, UPORT_0));
        vTaskDelay(pdMS_TO_TICKS(1));
        CHECK_RESULT(hal.enable(1, UPORT_1));
        vTaskDelay(pdMS_TO_TICKS(1));
        //vTaskDelay(pdMS_TO_TICKS(10));
        //ESP_LOGD(TAG, "%s port %d", 0?"Enabling":"Disabling", UPORT_0);
        CHECK_RESULT(hal.enable(0, UPORT_4));
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return CAN5_SUCCESS;
}
