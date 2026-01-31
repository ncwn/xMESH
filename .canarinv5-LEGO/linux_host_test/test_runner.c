/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   29/01/22
*
* File:  test_runner.c
* Descr: test business logic in host
*******************************************************************************/
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <can5_config.h>
#include <can5_storage_ram.h>
#include <stdlib.h>
#include <can5_storagemng.h>
#include <can5_utils.h>
#include <can5_netproto.h>
#include <can5_loramsg.h>
#include "can5_config.h"
#include <esp_log.h>
#include <can5_codec_hazemon.h>
#include <can5_http_json.h>
#include <can5_sensor_data.h>
#include <sys/time.h>
#include <can5_cron.h>
#include <can5_codec_mqtt.h>
#include "dict_storage_writer.h"
#include "lifo_storage_writer.h"
#include "unity.h"
#include "can5_codec_lwan.h"
#include "can5_storagemng.h"


#define MILLION         (1000000/100000)
#define SAMPLE_STRING   "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "\
                        "Vivamus sodales, risus ac molestie euismod, leo velit consectetur turpis, "\
                        "nec hendrerit nibh ex ac diam. Nunc ut bibendum augue. Duis id velit."

#define PRINT_ON

/* do something before tests start */
void setUp(void)
{
};

/* do something after tests stop */
void tearDown(void)
{
};

#ifdef PRINT_ON
#define CHECK_RET(x) ({ can5_err_t ret = x; if (ret != CAN5_SUCCESS ) CAN5_ERR_CHECK_NO_ABORT(ret); ret; })
#else
#define CHECK_RET(x) x
#endif

/************************************************************************************************
 * LIFO Storage lifo_storage_writer.c tests
 ************************************************************************************************/

void test_can5_storage_fs_lifo_push(void)
{
    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(can5_storage_lifo_push("testtag", "hello world 1", 13)));
    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(can5_storage_lifo_push("testtag", "hello world 2", 13)));
}

void test_can5_storage_fs_lifo_pop(void)
{
    char buffer[256];
    size_t len;

    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(can5_storage_lifo_pop("testtag", (uint8_t *) buffer, &len)));
    TEST_ASSERT_EQUAL_STRING("hello world 2", (char *) buffer);
    TEST_ASSERT_EQUAL(13, len);

    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(can5_storage_lifo_pop("testtag", (uint8_t *) buffer, &len)));
    TEST_ASSERT_EQUAL_STRING("hello world 1", (char *) buffer);
    TEST_ASSERT_EQUAL(13, len);

    TEST_ASSERT_EQUAL(CAN5_STORAGE_ERR_EMPTY, can5_storage_lifo_pop("testtag", (uint8_t *) buffer, &len));
}


void test_can5_storage_fs_lifo_stress_test(void)
{
    char buffer[1024];
    char pop_buffer[1024];
    size_t len, pop_len;

    for (int i = 0; i < MILLION; i++) {
        len = sprintf(buffer, "%d. %s", i, SAMPLE_STRING);
        TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(can5_storage_lifo_push("testtag", buffer, len)));
    }

    for (int i = MILLION - 1; i >= 0; i--) {
        len = sprintf(buffer, "%d. %s", i, SAMPLE_STRING);
        if (i % (MILLION / 5) == 0) {
            for (int j = 0; j < 10; j++) {
                TEST_ASSERT_EQUAL(CAN5_SUCCESS,
                                  CHECK_RET(can5_storage_lifo_peek("testtag", (uint8_t *) pop_buffer, &pop_len)));
                TEST_ASSERT_EQUAL_STRING(buffer, (char *) pop_buffer);
                TEST_ASSERT_EQUAL(len, pop_len);
            }
        }

        TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(can5_storage_lifo_pop("testtag", (uint8_t *) pop_buffer, &pop_len)));
        TEST_ASSERT_EQUAL_STRING(buffer, (char *) pop_buffer);
        TEST_ASSERT_EQUAL(len, pop_len);
    }

    TEST_ASSERT_EQUAL(CAN5_STORAGE_ERR_EMPTY, can5_storage_lifo_pop("testtag", (uint8_t *) buffer, NULL));
}

void test_can5_storage_fs_clear_old_lifo_stress_test(void)
{
    char buffer[1024];
    char pop_buffer[1024];
    size_t len, pop_len;


    for (int i = 0; i < 70; i++) {
        len = sprintf(buffer, "%d. %s", i, SAMPLE_STRING);
        TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(can5_storage_lifo_push("testtag", buffer, len)));
    }


    storage_manager.fs.remove_old_data("testtag");
}
/************************************************************************************************
 * Dictionary Storage dictionary_writer.c tests
 ************************************************************************************************/

void test_can5_storage_fs_dictionary_write(void)
{

    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(can5_storage_di_write("testcfg", "test1", "hello world 6", 13)));
    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(can5_storage_di_write("testcfg", "test2", "hello world 2", 13)));
    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(can5_storage_di_write("testcfg", "test8", "hello world 8", 13)));
    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(can5_storage_di_write("testcfg", "test9", "hello world 6", 13)));

    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(can5_storage_di_write("testcfg", "test1", "hello world 1", 13)));
    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(can5_storage_di_write("testcfg", "test9", "hello world 9", 13)));
}

void test_can5_storage_fs_dictionary_read(void)
{
    char buffer[1024] = { 1 };
    size_t buf_len;

    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(can5_storage_di_read("testcfg", "test1", buffer, &buf_len)));
    TEST_ASSERT_EQUAL_STRING("hello world 1", buffer);

    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(can5_storage_di_read("testcfg", "test9", buffer, &buf_len)));
    TEST_ASSERT_EQUAL_STRING("hello world 9", buffer);

}


void test_can5_storage_fs_dictionary_all(void)
{
    char buffer[1024]= { 68 };
    size_t buf_len;
    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(can5_storage_di_delete("testcfg", "test1")));
    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(can5_storage_di_delete("testcfg", "test2")));
    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(can5_storage_di_delete("testcfg", "test8")));
    TEST_ASSERT_EQUAL(CAN5_STORAGE_ERR_KEY_NOT_FOUND, can5_storage_di_read("testcfg", "test8", buffer, &buf_len));
    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(can5_storage_di_read("testcfg", "test9", buffer, &buf_len)));
    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(can5_storage_di_write("testcfg", "test2", "hello world 2", 13)));


    TEST_ASSERT_EQUAL(CAN5_SUCCESS,
                      CHECK_RET(can5_storage_di_write("testcfg", "samstr", SAMPLE_STRING, sizeof SAMPLE_STRING)));
    TEST_ASSERT_EQUAL(CAN5_SUCCESS, can5_storage_di_read("testcfg", "samstr", buffer, &buf_len));
    TEST_ASSERT_EQUAL_STRING(SAMPLE_STRING, buffer);
    TEST_ASSERT_EQUAL(strlen(SAMPLE_STRING), buf_len);
    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(can5_storage_di_delete("testcfg", "samstr")));
    TEST_ASSERT_EQUAL(CAN5_STORAGE_ERR_KEY_NOT_FOUND,
                      can5_storage_di_read("testcfg", "sample string", buffer, &buf_len));

    TEST_ASSERT_EQUAL(CAN5_SUCCESS,
                      CHECK_RET(can5_storage_di_write("testcfg", "samstr", SAMPLE_STRING, sizeof SAMPLE_STRING)));
    TEST_ASSERT_EQUAL(CAN5_SUCCESS, can5_storage_di_read("testcfg", "samstr", buffer, &buf_len));
    TEST_ASSERT_EQUAL_STRING(SAMPLE_STRING, buffer);
    TEST_ASSERT_EQUAL(strlen(SAMPLE_STRING), buf_len);
    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(can5_storage_di_delete("testcfg", "samstr")));
    TEST_ASSERT_EQUAL(CAN5_STORAGE_ERR_KEY_NOT_FOUND,
                      can5_storage_di_read("testcfg", "sample string", buffer, &buf_len));

}

/************************************************************************************************
 * Ram storage tests
 ************************************************************************************************/

void test_can5_storage_ram_test(void)
{
    char buffer[1024];
    size_t buf_len;
    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(storagedriv_ram.ops.init()));
    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(storagedriv_ram.ops.write("testtag2", "key1", "hello world 1", 13)));
    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(storagedriv_ram.ops.write("testtag2", "key2", "hello world 2", 13)));
    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(storagedriv_ram.ops.write("testtag2", "key3", "hello world 3", 13)));
    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(storagedriv_ram.ops.write("testtag1", "key1", "hello world 4", 13)));
    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(storagedriv_ram.ops.write("testtag1", "key2", "hello world 5", 13)));


    TEST_ASSERT_EQUAL(CAN5_SUCCESS,
                      CHECK_RET(storagedriv_ram.ops.read("testtag2", "key2", (uint8_t *) buffer, &buf_len)));
    TEST_ASSERT_EQUAL_STRING("hello world 2", buffer);

    TEST_ASSERT_EQUAL(CAN5_SUCCESS,
                      CHECK_RET(storagedriv_ram.ops.read("testtag1", "key2", (uint8_t *) buffer, &buf_len)));
    TEST_ASSERT_EQUAL_STRING("hello world 5", buffer);

    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(storagedriv_ram.ops.remove("testtag1", "key2")));

    TEST_ASSERT_EQUAL(CAN5_STORAGE_ERR_INVALID_INDEX,
                      storagedriv_ram.ops.read("testtag1", "key2", (uint8_t *) buffer, &buf_len));

    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(storagedriv_ram.ops.remove("testtag2", "key1")));

    TEST_ASSERT_EQUAL(CAN5_STORAGE_ERR_INVALID_INDEX,
                      storagedriv_ram.ops.read("testtag2", "key1", (uint8_t *) buffer, &buf_len));

    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(storagedriv_ram.ops.remove("testtag2", "key3")));

    TEST_ASSERT_EQUAL(CAN5_STORAGE_ERR_INVALID_INDEX,
                      storagedriv_ram.ops.read("testtag2", "key3", (uint8_t *) buffer, &buf_len));

    TEST_ASSERT_EQUAL(CAN5_SUCCESS,
                      CHECK_RET(storagedriv_ram.ops.read("testtag2", "key2", (uint8_t *) buffer, &buf_len)));
    TEST_ASSERT_EQUAL_STRING("hello world 2", buffer);
}

void test_can5_storage_ram_stack_test()
{
    char buffer[1024];
    char pop_buffer[1024];
    size_t len, pop_len;
    size_t ram_max = 64;
    const char tag[4][8] = {
        "tag1",
        "tag2",
        "tag3",
        "tag4"
    };
    for (int i = 0; i < ram_max; i++) {
        len = sprintf(buffer, "%d. %s", i, SAMPLE_STRING);
        TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(storagedriv_ram.ops.push(tag[i % 4], buffer, len)));
    }

    for (int i = ram_max - 1; i >= 0; i--) {
        len = sprintf(buffer, "%d. %s", i, SAMPLE_STRING);
        if (i % (ram_max / 5) == 0) {
            for (int j = 0; j < 10; j++) {
                TEST_ASSERT_EQUAL(CAN5_SUCCESS,
                                  CHECK_RET( storagedriv_ram.ops.peek(tag[i % 4], (uint8_t *) pop_buffer, &pop_len)));
                TEST_ASSERT_EQUAL_STRING(buffer, (char *) pop_buffer);
                TEST_ASSERT_EQUAL(len, pop_len);
            }
        }

        TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET( storagedriv_ram.ops.pop(tag[i % 4], (uint8_t *) pop_buffer, &pop_len)));
        TEST_ASSERT_EQUAL_STRING(buffer, (char *) pop_buffer);
        TEST_ASSERT_EQUAL(len, pop_len);
    }

    TEST_ASSERT_EQUAL(CAN5_STORAGE_ERR_EMPTY, storagedriv_ram.ops.peek(tag[0], (uint8_t *) buffer, NULL));
    TEST_ASSERT_EQUAL(CAN5_STORAGE_ERR_EMPTY, storagedriv_ram.ops.peek(tag[1], (uint8_t *) buffer, NULL));
    TEST_ASSERT_EQUAL(CAN5_STORAGE_ERR_EMPTY, storagedriv_ram.ops.peek(tag[2], (uint8_t *) buffer, NULL));
    TEST_ASSERT_EQUAL(CAN5_STORAGE_ERR_EMPTY, storagedriv_ram.ops.peek(tag[3], (uint8_t *) buffer, NULL));

}



/************************************************************************************************
 * can5 config tests
 ************************************************************************************************/

void test_can5_config_module_init(void)
{
    config_manager.write(CFG_INIT, "false", strlen("false"));
    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(config_manager.module.init()));
}

void test_can5_config_get_io(void)
{
    can5_cfg_res_t config;
    size_t config_len;

    config_manager.write(CFG_UART_3, "CAN5_SENSORDRIV_TYPE_CO2", 24);
    config_manager.write(CFG_UART_4, "CAN5_SENSORDRIV_TYPE_GPS", 24);
    config_manager.write(CFG_ADC_3, "CAN5_SENSORDRIV_TYPE_CO", 23);

    config = config_manager.get_config(CAN5_CFG_REQ_IO_PORTS, &config_len);

    TEST_ASSERT_NOT_NULL(config.io_ports);
    TEST_ASSERT_EQUAL(CFG_UART_7 - CFG_ADC_0 + 1, config_len); // 4 + 8 + 8 ports

    for(int i = 0; i < config_len; i ++) {
        //TEST_ASSERT_EQUAL(i + CFG_ADC_0, config.io_ports[i].port);
        if (config.io_ports[i].port == UPORT_3) {
            TEST_ASSERT_EQUAL(CAN5_SENSORDRIV_TYPE_CO2, config.io_ports[i].sensor);
        }
        else if (config.io_ports[i].port == UPORT_4) {
            TEST_ASSERT_EQUAL(CAN5_SENSORDRIV_TYPE_GPS, config.io_ports[i].sensor);
        }
        else if (config.io_ports[i].port == ADPORT_3) {
            TEST_ASSERT_EQUAL(CAN5_SENSORDRIV_TYPE_CO, config.io_ports[i].sensor);
        }
        else {
            TEST_ASSERT_FALSE(config.io_ports[i].is_sensor);
        }

    }
    config_manager.free_config(CAN5_CFG_REQ_IO_PORTS, &config);
}


void test_can5_http_json_get(void)
{
    char *json, *res;

    json = can5_http_json_str_get(CAN5_HTTP_JSON_IO);

#ifdef PRINT_ON
    printf("IO JSON get: %s\n", json);
#endif
    free(json);

    json = can5_http_json_str_get(CAN5_HTTP_JSON_LORAWAN);

#ifdef PRINT_ON
    printf("LWAN JSON get: %s\n", json);
#endif

    strstr(json, "5")[0] = 'a';
    res = can5_http_json_set(CAN5_HTTP_JSON_LORAWAN, json);
#ifdef PRINT_ON
    printf("LWAN get: %s\n", res);
#endif
    free(res);
    free(json);
}

void storage_pop_and_print(const char *tag, int arr[], size_t arr_len)
{
    char buffer[CAN5_STORAGE_MAX_LEN] = { 0 };
    size_t buffer_len = 0;
    int i =0;
    can5_err_t ret;
    while ((ret = storage_manager.fs.pop(tag, (uint8_t *)buffer, &buffer_len)) == CAN5_SUCCESS) {
        if (i < arr_len) {
            char tmp[CAN5_STORAGE_MAX_LEN] = { 0 };
            sprintf(tmp, "hello %i", arr[i]);
            TEST_ASSERT_EQUAL_STRING(tmp, buffer);
        }
        else {
            TEST_FAIL_MESSAGE("More items than required.");
        }
#ifdef PRINT_ON
        printf("%i. Popping \"%s\" %d\n", i, buffer, buffer_len);
#endif
        i++;
    }

#ifdef PRINT_ON
    CAN5_ERR_CHECK_NO_ABORT(ret);
#endif
}

void storage_print_and_push(const char *tag, int arr[], size_t arr_len)
{
    char buffer[CAN5_STORAGE_MAX_LEN] = { 0 };
    size_t buffer_len = 0;
    int i =0;
    can5_err_t ret;

    for(i = 0; i < arr_len; i++) {
        sprintf(buffer, "hello %i", arr[i]);
#ifdef PRINT_ON
        printf("%i. Pushing \"%s\" %d\n",i , buffer, strlen(buffer));
#endif
        TEST_ASSERT_EQUAL(CAN5_SUCCESS, storage_manager.fs.push(tag, (uint8_t  *)buffer, strlen(buffer)));
    }

}

void test_can5_search_and_pop(void)
{

    // remove all items
    int arr_1[] = {
        1, 2, 3, 4, 5, 6
    };
    storage_print_and_push("data", arr_1, 6);

    storage_manager.fs.search_and_pop("data", (uint8_t *)"hello 6", strlen("hello 6"), 10);

    int arr_2[] = {
        5, 4,  3, 2, 1
    };
    storage_pop_and_print("data",arr_2, 5);

    storage_print_and_push("data", arr_1, 6);
    storage_manager.fs.search_and_pop("data", (uint8_t *)"hello 5", strlen("hello 5"), 10);

    int arr_3[] = {
        6, 4, 3, 2, 1
    };
    storage_pop_and_print("data",arr_3, 5);

    storage_print_and_push("data", arr_1, 6);
    storage_manager.fs.search_and_pop("data", (uint8_t *)"hello 4", strlen("hello 4"), 10);

    int arr_4[] = {
        6, 5, 3, 2, 1
    };
    storage_pop_and_print("data",arr_4, 5);

    storage_print_and_push("data", arr_1, 6);
    storage_manager.fs.search_and_pop("data", (uint8_t *)"hello 1", strlen("hello 1"), 10);

    int arr_5[] = {
        6, 5, 4, 3, 2
    };
    storage_pop_and_print("data",arr_5, 5);

    storage_print_and_push("data", arr_1, 6);
    storage_manager.fs.search_and_pop("data", (uint8_t *)"hello 0", strlen("hello 1"), 10);

    int arr_6[] = {
        6, 5, 4, 3, 2, 1
    };
    storage_pop_and_print("data",arr_6, 6);

    storage_print_and_push("data", arr_1, 6);
    storage_manager.fs.search_and_pop("data", (uint8_t *)"hello 0", strlen("hello 1"), 4);

    int arr_7[] = {
        6, 5, 4, 3, 2, 1
    };
    storage_pop_and_print("data",arr_7, 6);
}

void test_can5_sensor_data(void)
{
    const char * sensor_data_str1 = "TIMESTAMP:none:n:1234546, PMS7003_PM1_0_CF1:uart1:n:334,  BME280_TEMP:adc3:d:11.3";
    const char * sensor_data_str2 = "TIMESTAMP:none:n:1234546, PMS7003_PM1_0_CF1:uart2:n:334,  BME280_TEMP:adc4:d:11.3";
    char result[CAN5_STORAGE_MAX_LEN];
    can5_sensor_data_list_t list1, list2;
    TAILQ_INIT(&list1);
    TAILQ_INIT(&list2);

    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(can5_sensor_data_list_loads(sensor_data_str1, &list1)));

    can5_sensor_data_t * sensor_data_no2 = can5_sensor_data_loads("ZE03_NO2:uart0:s:11.2");
    TEST_ASSERT_NOT_NULL(sensor_data_no2);

    TAILQ_INSERT_HEAD(&list1, sensor_data_no2, te);
    can5_sensor_data_t *elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_PMS7003_PM1_0_ATM, I2C_0,
                                                        CAN5_SENSOR_DATA_DATATYPE_STR, "1234", 0, 0);
    TEST_ASSERT_NOT_NULL(elem);

    TAILQ_INSERT_HEAD(&list1, elem, te);

    CLEAR_ARRAY(result);
    can5_sensor_data_list_dumps(&list1, result);

    const char *expected1 = "TIMESTAMP:none:n:1234546, PMS7003_PM1_0_CF1:uart1:n:334, PMS7003_PM1_0_ATM:i2c:s:1234, ZE03_NO2:uart0:s:11.2, BME280_TEMP:adc3:d:11.3";
    TEST_ASSERT_EQUAL_STRING(expected1, result);

#ifdef PRINT_ON
    printf("%s\n", result);
#endif

    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(can5_sensor_data_list_loads(sensor_data_str2, &list2)));

    TAILQ_CONCAT(&list1, &list2, te);

    CLEAR_ARRAY(result);
    can5_sensor_data_list_dumps(&list1, result);

    const char *expected2 = "TIMESTAMP:none:n:1234546, TIMESTAMP:none:n:1234546, PMS7003_PM1_0_CF1:uart1:n:334, PMS7003_PM1_0_CF1:uart2:n:334, PMS7003_PM1_0_ATM:i2c:s:1234, ZE03_NO2:uart0:s:11.2, BME280_TEMP:adc3:d:11.3, BME280_TEMP:none:d:11.3";
    TEST_ASSERT_EQUAL_STRING(expected2, result);

#ifdef PRINT_ON
    printf("%s\n", result);
#endif

    can5_sensor_data_list_free(&list1);
}

/************************************************************************************************
 * HAZEMON codec tests
 ************************************************************************************************/
void test_can5_hazemon_encoder(void)
{
    uint8_t result[CAN5_STORAGE_MAX_LEN];
    size_t result_len;
    const char *in_str = "TIMESTAMP:none:n:1658926856, UBLOX_NEO_GPS_LAT:uart6:d:14.0775015, UBLOX_NEO_GPS_LNG:uart6:d:100.613097, UBLOX_NEO_GPS_ALT:uart6:d:-2.8, PMS7003_PM1_0_CF1:uart1:d:20, PMS7003_PM2_5_CF1:uart1:d:27, PMS7003_PM10_CF1:uart1:d:29, PMS7003_PM1_0_ATM:uart1:d:19, PMS7003_PM2_5_ATM:uart1:d:26, PMS7003_PM10_ATM:uart1:d:29, MH_Z16_CO2:uart3:d:463, ZE07_CO:adc0:n:142, BME280_TEMP:none:d:27.330000, BME280_PRES:none:d:1007.102478, BME280_HUMI:none:d:50.744141";
    const char *sensor_id = "39628670564992";
    const char *project_id = "1";
    can5_sensor_data_list_t list;

    config_manager.write(CFG_DEVICE_ID, (const uint8_t *)sensor_id, strlen(sensor_id));
    config_manager.write(CFG_HAZEMON_PROJECT_ID, (const uint8_t *)project_id, strlen(project_id));

    TAILQ_INIT(&list);

    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(can5_sensor_data_list_loads(in_str, &list)));
    CLEAR_ARRAY(result);
    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(hazemon_make_tx_packet(&list, result, &result_len)));
    // in little endian
    const uint8_t expected_packet[] = {
        0xff, 0x01, 0xc9, 0x51, 0xb0, 0x6b, 0x68, 0x01, 0x00, //device id
        0x08, 0x37, 0xe1, 0x62, 0x00, 0x00, 0x00, 0x00, // timestamp
        0x46, 0x00, //pkt size
        0x01, 0x04, 0x72, 0x3d, 0x61, 0x41, // LAT
        0x02, 0x04, 0xe8, 0x39, 0xc9, 0x42, // LNG
        0x03, 0x02, 0xfe, 0xff,  // ALT
        0x09, 0x02, 0x14, 0x00,  // PM 1.0
        0x07, 0x02, 0x1b, 0x00,  // PM 2.5
        0x08, 0x02, 0x1d, 0x00,  // PM 10
        0x18, 0x02, 0xcf, 0x01,  // CO2
        0x0e, 0x04, 0x00, 0x80, 0xb1, 0x44,  // CO
        0x04, 0x02, 0x11, 0x01, // TEMP
        0x06, 0x02, 0x57, 0x27, // PRES
        0x05, 0x02, 0xFB, 0x01, // HUMI
        0x82, // CRC
    };

    TEST_ASSERT_EQUAL(70, result_len);

    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_packet, result, result_len);

    can5_sensor_data_list_free(&list);
    TAILQ_INIT(&list);

    can5_sensor_data_t *sensor_data = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_TIMESTAMP, CAN5_PORT_NULL, CAN5_SENSOR_DATA_DATATYPE_NUM, 0, 0, 0);
    TAILQ_INSERT_HEAD(&list, sensor_data, te);

    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(hazemon_make_tx_packet(&list, result, &result_len)));

    TEST_ASSERT_EQUAL(20, result_len);

    const uint8_t expected_hello_pkt[] = {
        0xff, 0x01, 0xc9, 0x51, 0xb0, 0x6b, 0x68, 0x01, 0x00, //device id
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // timestamp
        0x14, 0x00, //pkt size
        0x2d
    };

    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_hello_pkt, result, result_len);
    can5_sensor_data_list_free(&list);
}

void test_can5_hazemon_decoder(void)
{
    hazemon_rx_cmd_list_t list;
    const char *sensor_id = "39628670564992";
    const char *project_id = "1";
    config_manager.write(CFG_DEVICE_ID, (const uint8_t *)sensor_id, strlen(sensor_id));
    config_manager.write(CFG_HAZEMON_PROJECT_ID, (const uint8_t *)project_id, strlen(project_id));

    TAILQ_INIT(&list);

    const uint8_t rx_buffer[] = {
        0x00,                                                   // manditory  0x00
        0xff, 0x01, 0xc9, 0x51, 0xb0, 0x6b, 0x68, 0x01, 0x00,   // device id
        0x9e, 0xca, 0xf8, 0x61, 0x00, 0x00, 0x00, 0x00,         // server timestamp
        0x18, 0x00,                                             // pkt len
        0x00, 0x01, 0x05,                                       // interval 5 minute
        0x1d                                                    // 1 byte maxim crc
    };

    TEST_ASSERT_EQUAL(CAN5_SUCCESS,
                      CHECK_RET(hazemon_parse_rx_packet(rx_buffer, sizeof(rx_buffer), &list)));

    TEST_ASSERT_EQUAL(HAZEMON_INTERVAL, TAILQ_FIRST(&list)->type);
    TEST_ASSERT_EQUAL_STRING("5", TAILQ_FIRST(&list)->val);

    hazemon_rx_cmd_t *cur;
    int num_cmds = 0;

    TAILQ_FOREACH(cur, &list, te) {
        num_cmds++;
    }

    TEST_ASSERT_EQUAL(1, num_cmds);

    hazemon_rx_cmd_list_free(&list, false);
}

void test_can5_lwan_encoder(void)
{
    const char *in_str = "TIMESTAMP:none:n:1658926856, UBLOX_NEO_GPS_LAT:uart6:d:14.0775015, UBLOX_NEO_GPS_LNG:uart6:d:100.613097, UBLOX_NEO_GPS_ALT:uart6:d:-2.8, PMS7003_PM1_0_CF1:uart1:d:20, PMS7003_PM2_5_CF1:uart1:d:27, PMS7003_PM10_CF1:uart1:d:29, PMS7003_PM1_0_ATM:uart1:d:19, PMS7003_PM2_5_ATM:uart1:d:26, PMS7003_PM10_ATM:uart1:d:29, MH_Z16_CO2:uart3:d:463, ZE07_CO:adc0:d:142, BME280_TEMP:none:d:27.330000, BME280_PRES:none:d:1007.102478, BME280_HUMI:none:d:50.744141";
    can5_sensor_data_list_t list;
    double f_data;

    TAILQ_INIT(&list);
    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(can5_sensor_data_list_loads(in_str, &list)));


    lwan_tx_pkt_list_t lwan_pkts;
    CLEAR_STRUCT(lwan_pkts);

    lwan_make_tx_packets_args_t args = {
        .cycle_ctr = 1,
        .skip_gps = false,
        .datarate = 2,
    };
    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(lwan_make_tx_packets(&list, &lwan_pkts, &args)));

    TEST_ASSERT_EQUAL(9, lwan_pkts.count);

    for (size_t i = 0; i < lwan_pkts.count; i++) {
        struct can5_lmsg_packet *s_packet = lwan_pkts.pkt[i].pkt;
        can5_lmsg_ntoh_packet(s_packet, lwan_pkts.pkt->len);
        TEST_ASSERT_EQUAL(1658926856, can5_lmsg_get_packet_timestamp(s_packet));
#ifdef PRINT_ON
        printf("Num: %d ****PKT**** Pkt size: %d Timestamp: %u\n", i, lwan_pkts.pkt->len, can5_lmsg_get_packet_timestamp(s_packet));
#endif
        for (uint8_t k = 0; k < can5_lmsg_get_packet_n_data(s_packet); k++) {
            struct can5_lmsg_data *sd = can5_lmsg_parse_packet(s_packet, k);
            f_data = 0;
            TEST_ASSERT_EQUAL(args.cycle_ctr, sd->cycle_id);
            switch (sd->seq) {
                case 0:
                    TEST_ASSERT_EQUAL(LMSG_GLAT, sd->sensor_type);
                    f_data = can5_lmsg_parse_sensor_data(sd);
                    TEST_ASSERT_EQUAL_FLOAT(14.0775015, f_data);
                    break;
                case 1:
                    TEST_ASSERT_EQUAL(LMSG_GLNG, sd->sensor_type);
                    f_data = can5_lmsg_parse_sensor_data(sd);
                    TEST_ASSERT_EQUAL_FLOAT(100.613097, f_data);
                    break;
                case 2:
                    TEST_ASSERT_EQUAL(LMSG_GALT, sd->sensor_type);
                    f_data = can5_lmsg_parse_sensor_data(sd);
                    TEST_ASSERT_EQUAL_FLOAT(-2.8, f_data);
                    break;
                case 3:
                    TEST_ASSERT_EQUAL(LMSG_PM1, sd->sensor_type);
                    f_data = can5_lmsg_parse_sensor_data(sd);
                    TEST_ASSERT_EQUAL_FLOAT(20, f_data);
                    break;
                case 4:
                    TEST_ASSERT_EQUAL(LMSG_PM2_5, sd->sensor_type);
                    f_data = can5_lmsg_parse_sensor_data(sd);
                    TEST_ASSERT_EQUAL_FLOAT(27, f_data);
                    break;
                case 5:
                    TEST_ASSERT_EQUAL(LMSG_PM10, sd->sensor_type);
                    f_data = can5_lmsg_parse_sensor_data(sd);
                    TEST_ASSERT_EQUAL_FLOAT(29, f_data);
                    break;
                case 6:
                    TEST_ASSERT_EQUAL(LMSG_MHZ16CO2, sd->sensor_type);
                    f_data = can5_lmsg_parse_sensor_data(sd);
                    TEST_ASSERT_EQUAL_FLOAT(463, f_data);
                    break;
                case 7:
                    TEST_ASSERT_EQUAL(LMSG_CO, sd->sensor_type);
                    f_data = can5_lmsg_parse_sensor_data(sd);
                    TEST_ASSERT_EQUAL_FLOAT(142, f_data);                                                // co which is supposed to be float get multiplied by 10
                    break;
                case 8:
                    TEST_ASSERT_EQUAL(LMSG_TEMP, sd->sensor_type);
                    f_data = can5_lmsg_parse_sensor_data(sd);
                    TEST_ASSERT_EQUAL_FLOAT(27.3, f_data);
                    break;
                case 9:
                    TEST_ASSERT_EQUAL(LMSG_PRES, sd->sensor_type);
                    f_data = can5_lmsg_parse_sensor_data(sd);
                    TEST_ASSERT_EQUAL_FLOAT(1007.1, f_data);
                    break;
                case 10:
                    TEST_ASSERT_EQUAL(LMSG_HUMI, sd->sensor_type);
                    f_data = can5_lmsg_parse_sensor_data(sd);
                    TEST_ASSERT_EQUAL_FLOAT(50.7, f_data);
                    break;
            }
#ifdef PRINT_ON
            printf("Sensor:[%u] Type %s Data %lf\n", sd->seq, loramsg_sensor_type_str(sd->sensor_type),
                   f_data);
#endif
        }
        can5_lmsg_free_packet(s_packet);
    }


    args.datarate = 3;
    args.cycle_ctr = 2;
    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(lwan_make_tx_packets(&list, &lwan_pkts, &args)));

    TEST_ASSERT_EQUAL(1, lwan_pkts.count);

    for (size_t i = 0; i < lwan_pkts.count; i++) {
        struct can5_lmsg_packet *s_packet = lwan_pkts.pkt[i].pkt;
        can5_lmsg_ntoh_packet(s_packet, lwan_pkts.pkt->len);
        TEST_ASSERT_EQUAL(1658926856, can5_lmsg_get_packet_timestamp(s_packet));
#ifdef PRINT_ON
        printf("Num: %d ****PKT**** Pkt size: %d Timestamp: %u\n", i, lwan_pkts.pkt->len, can5_lmsg_get_packet_timestamp(s_packet));
#endif
        for (uint8_t k = 0; k < can5_lmsg_get_packet_n_data(s_packet); k++) {
            struct can5_lmsg_data *sd = can5_lmsg_parse_packet(s_packet, k);
            f_data = 0;
            TEST_ASSERT_EQUAL(args.cycle_ctr, sd->cycle_id);
            switch (sd->seq) {
                case 0:
                    TEST_ASSERT_EQUAL(LMSG_GLAT, sd->sensor_type);
                    f_data = can5_lmsg_parse_sensor_data(sd);
                    TEST_ASSERT_EQUAL_FLOAT(14.0775015, f_data);
                    break;
                case 1:
                    TEST_ASSERT_EQUAL(LMSG_GLNG, sd->sensor_type);
                    f_data = can5_lmsg_parse_sensor_data(sd);
                    TEST_ASSERT_EQUAL_FLOAT(100.613097, f_data);
                    break;
                case 2:
                    TEST_ASSERT_EQUAL(LMSG_GALT, sd->sensor_type);
                    f_data = can5_lmsg_parse_sensor_data(sd);
                    TEST_ASSERT_EQUAL_FLOAT(-2.8, f_data);
                    break;
                case 3:
                    TEST_ASSERT_EQUAL(LMSG_PM1, sd->sensor_type);
                    f_data = can5_lmsg_parse_sensor_data(sd);
                    TEST_ASSERT_EQUAL_FLOAT(20, f_data);
                    break;
                case 4:
                    TEST_ASSERT_EQUAL(LMSG_PM2_5, sd->sensor_type);
                    f_data = can5_lmsg_parse_sensor_data(sd);
                    TEST_ASSERT_EQUAL_FLOAT(27, f_data);
                    break;
                case 5:
                    TEST_ASSERT_EQUAL(LMSG_PM10, sd->sensor_type);
                    f_data = can5_lmsg_parse_sensor_data(sd);
                    TEST_ASSERT_EQUAL_FLOAT(29, f_data);
                    break;
                case 6:
                    TEST_ASSERT_EQUAL(LMSG_MHZ16CO2, sd->sensor_type);
                    f_data = can5_lmsg_parse_sensor_data(sd);
                    TEST_ASSERT_EQUAL_FLOAT(463, f_data);
                    break;
                case 7:
                    TEST_ASSERT_EQUAL(LMSG_CO, sd->sensor_type);
                    f_data = can5_lmsg_parse_sensor_data(sd);
                    TEST_ASSERT_EQUAL_FLOAT(142, f_data);
                    break;
                case 8:
                    TEST_ASSERT_EQUAL(LMSG_TEMP, sd->sensor_type);
                    f_data = can5_lmsg_parse_sensor_data(sd);
                    TEST_ASSERT_EQUAL_FLOAT(27.3, f_data);
                    break;
                case 9:
                    TEST_ASSERT_EQUAL(LMSG_PRES, sd->sensor_type);
                    f_data = can5_lmsg_parse_sensor_data(sd);
                    TEST_ASSERT_EQUAL_FLOAT(1007.1, f_data);
                    break;
                case 10:
                    TEST_ASSERT_EQUAL(LMSG_HUMI, sd->sensor_type);
                    f_data = can5_lmsg_parse_sensor_data(sd);
                    TEST_ASSERT_EQUAL_FLOAT(50.7, f_data);
                    break;
            }
#ifdef PRINT_ON
            printf("Sensor:[%u] Type %s Data %d %lf\n", sd->seq, loramsg_sensor_type_str(sd->sensor_type),
                   f_data);
#endif
        }
        can5_lmsg_free_packet(s_packet);
    }
    can5_sensor_data_list_free(&list);
}

void test_can5_cron(void)
{
    can5_cron_job_t job;
    time_t cur_time = time(NULL);
    time_t next_time;


    can5_cron_init();

    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL(CAN5_SUCCESS, can5_cron_next_time(cur_time, &next_time));
        cur_time = next_time;
        can5_cron_run_jobs(cur_time);
    }


    can5_cron_free();
}

void test_mqtt_codec(void)
{
    const char *in_str = "TIMESTAMP:none:n:1658926856, UBLOX_NEO_GPS_LAT:uart6:d:14.0775015, UBLOX_NEO_GPS_LNG:uart6:d:100.613097, UBLOX_NEO_GPS_ALT:uart6:d:-2.8, PMS7003_PM1_0_CF1:uart1:d:20, PMS7003_PM2_5_CF1:uart1:d:27, PMS7003_PM10_CF1:uart1:d:29, PMS7003_PM1_0_ATM:uart1:d:19, PMS7003_PM2_5_ATM:uart1:d:26, PMS7003_PM10_ATM:uart1:d:29, MH_Z16_CO2:uart3:d:463, ZE07_CO:adc0:n:142, BME280_TEMP:none:d:27.330000, BME280_PRES:none:d:1007.102478, BME280_HUMI:none:d:50.744141";
    const char *sensor_id = "39628670564992";
    const char *project_id = "1";
    const char *organization = "interlab";
    can5_sensor_data_list_t list;
    mqtt_tx_list_t out_list;
    mqtt_tx_t *elem;

    config_manager.write(CFG_DEVICE_ID, (const uint8_t *)sensor_id, strlen(sensor_id));
    config_manager.write(CFG_DEVICE_ORGANIZATION, (const uint8_t *)organization, strlen(organization));
    config_manager.write(CFG_HAZEMON_PROJECT_ID, (const uint8_t *)project_id, strlen(project_id));

    TAILQ_INIT(&list);
    TAILQ_INIT(&out_list);

    TEST_ASSERT_EQUAL(CAN5_SUCCESS, CHECK_RET(can5_sensor_data_list_loads(in_str, &list)));

    mqtt_make_tx(&list, &out_list, "interlab");

    can5_sensor_data_list_free(&list);

    TAILQ_FOREACH(elem, &out_list, te) {
        printf("Topic: %s, Data %s Retain:%s\n", elem->topic, elem->data, boolean_get_str(elem->retain));
    }
    mqtt_tx_list_free(&out_list);
}

int main()
{
    system("./pre_test.sh");
    UNITY_BEGIN();
    storage_manager.module.init();
    RUN_TEST(test_can5_config_module_init);
    RUN_TEST(test_can5_storage_fs_lifo_push);
    RUN_TEST(test_can5_storage_fs_lifo_pop);
    RUN_TEST(test_can5_storage_fs_lifo_stress_test);
    RUN_TEST(test_can5_storage_fs_clear_old_lifo_stress_test);

    RUN_TEST(test_can5_storage_fs_dictionary_write);
    RUN_TEST(test_can5_storage_fs_dictionary_read);
    RUN_TEST(test_can5_storage_fs_dictionary_all);

    RUN_TEST(test_can5_storage_ram_test);
    RUN_TEST(test_can5_storage_ram_stack_test);


    //RUN_TEST(test_can5_netcodec_hazemon_make_packet);
    //RUN_TEST(test_can5_netcodec_hazemon_parse_packet);

    RUN_TEST(test_can5_config_get_io);

    RUN_TEST(test_can5_http_json_get);

    RUN_TEST(test_can5_search_and_pop);

    RUN_TEST(test_can5_sensor_data);

    RUN_TEST(test_can5_hazemon_encoder);
    RUN_TEST(test_can5_hazemon_decoder);

    RUN_TEST(test_can5_lwan_encoder);
    RUN_TEST(test_can5_cron);

    RUN_TEST(test_mqtt_codec);

    can5_storage_di_unregister_wearlevel();

    UNITY_END();
}
