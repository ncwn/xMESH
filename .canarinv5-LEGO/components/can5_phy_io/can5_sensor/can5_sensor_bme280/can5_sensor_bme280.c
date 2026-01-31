/*******************************************************************************
* Author: Kalana Jayaratne @kalanaj ,Raunak Mukhia @rmukhia,
* Date:   12/01/22
*
* File:  can5_sensor_bme280.c
* Descr:
*******************************************************************************/

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>
#include "can5_hazemon_types.h"
#include "esp_log.h"
#include "can5_error.h"
#include "can5_utils.h"
#include "can5_hal.h"
#include "can5_sensor_bme280.h"


static const char *TAG = "SENSOR_BME280";

#define CAN5_BME280_SENSOR_ADDR                 0x76

#define CAN5_BME280_REG_CALIB_00                0x88
#define CAN5_BME280_REG_CHIP_ID                 0xD0
#define CAN5_BME280_REG_RESET                   0XE0
#define CAN5_BME280_REG_CALIB_26                0xE1
#define CAN5_BME280_REG_CTRL_HUM                0xF2 
#define CAN5_BME280_REG_STATUS                  0xF3
#define CAN5_BME280_REG_CTRL_MEAS               0xF4
#define CAN5_BME280_REG_CONFIG                  0xF5
#define CAN5_BME280_REG_ADC_BURST_READ_START    0xF7 //press_msb

#define CAN5_BME280_CALIB_DATA_LEN              32
#define CAN5_BME280_BURST_READ_LEN              8

#define CHECK_INITD(s_hdl) do {                       \
    if ((s_hdl)->status == SENSORDRIV_BME280_STAT_UNINITD) return CAN5_ERR_INVALID_STATE; \
} while(0)

#if 0
#define TRACE_FUNC ESP_LOGI(TAG, "in -> %s() :%d", __FUNCTION__, __LINE__)
#else
#define TRACE_FUNC
#endif

// static uint8_t CALIB_DATA[CAN5_BME280_CALIB_DATA_LEN];
typedef struct can5_sensor_bme280_comp_s{
    uint16_t        dig_T1;
    int16_t         dig_T2;
    int16_t         dig_T3;
    uint16_t        dig_P1;
    int16_t         dig_P2;
    int16_t         dig_P3;
    int16_t         dig_P4;
    int16_t         dig_P5;
    int16_t         dig_P6;
    int16_t         dig_P7;
    int16_t         dig_P8;
    int16_t         dig_P9;
    unsigned char   dig_H1;
    int16_t         dig_H2;
    unsigned char   dig_H3;
    int16_t         dig_H4;
    int16_t         dig_H5;
    char            dig_H6;
} can5_sensor_bme280_comp_t;


typedef struct can5_sensor_bme280_adc_s{
    int32_t         adc_P;
    int32_t         adc_T;
    int32_t         adc_H;
}can5_sensor_bme280_adc_t;


typedef enum sensordriv_bme280_status_e {
    SENSORDRIV_BME280_STAT_UNINITD = 0,
    SENSORDRIV_BME280_STAT_INITD,
    SENSORDRIV_BME280_STAT_READING,
} sensordriv_bme280_status_t;

static can5_sensor_hdl_t *alloc(size_t *len);

static uint8_t get_sensor_id(can5_sensor_hdl_t *hdl);

static void set_sensor_id(can5_sensor_hdl_t *hdl, uint8_t sensor_id);

static can5_err_t detect(can5_sensor_hdl_t *hdl, can5_port_idx_t port);

static can5_err_t init(can5_sensor_hdl_t *hdl, can5_port_idx_t port, can5_sensor_hwcb_f *hwcb);

static can5_err_t uninit(can5_sensor_hdl_t *hdl);

static can5_err_t run(can5_sensor_hdl_t *hdl);

static can5_err_t read(can5_sensor_hdl_t *hdl, void *prxdata, size_t *plen, time_t timeout, bool blocking);

static can5_err_t register_read_cb(can5_sensor_hdl_t *hdl, can5_sensor_readcb_f *);

static can5_err_t enable(can5_sensor_hdl_t *hdl, bool enable);

static can5_sensor_data_list_t *get_sensor_data(can5_sensor_hdl_t *hdl, const void *data, size_t data_len);

static bool is_running(can5_sensor_hdl_t *hdl);

static can5_err_t driverctl(can5_sensor_hdl_t *hdl, uint8_t request, const void *params, void *response);

static int32_t status_get(can5_sensor_hdl_t *hdl);

#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL > 0

static const char *status_getstr(can5_sensor_hdl_t *hdl, int32_t status);

#endif


const can5_sensordriv_t sensordriv_bme280 = {
    .ops = {
        .alloc = alloc,
        .get_id = get_sensor_id,
        .set_id = set_sensor_id,
        .detect = detect,
        .init = init,
        .uninit = uninit,
        .run = run,
        .read = read,
        .register_read_cb = register_read_cb,
        .enable = enable,
        .get_sensor_data = get_sensor_data,
        .is_running = is_running,
        .driverctl = driverctl,
        .status_get = status_get,
#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL > 0
        .status_getstr = status_getstr,
#endif
    },
    .details = {
        .sensor = {
            .init_warm_up = CAN5_SENSOR_TIME_MIN,
            .read_warm_up = CAN5_SENSOR_TIME_MIN,
            .read_time = 60,
            .type = CAN5_SENSORDRIV_TYPE_BME280,
        },
        .io_type = CAN5_PHY_IO_TYPE_I2C,
        .name = "BME280",
        .version = "1.0",
        .manufacturer = "Bosch Sensortec",
    }
};

typedef struct sensor_hdl_s {
    uint8_t sensor_id;
    volatile sensordriv_bme280_status_t  status;
    volatile bool async_read;
    can5_port_idx_t port;
    can5_sensor_readcb_f *readcb;
    can5_sensor_bme280_data_t cb_data;
    can5_sensor_hwcb_f  *hwcb;
    can5_sensor_bme280_comp_t comp_data;
    can5_sensor_bme280_adc_t adc_data;
    int32_t t_fine;
    time_t read_time_end;
    struct {
      can5_sensor_bme280_data_t data;
      ssize_t n;
    } read;
} sensor_hdl_t;

#define SENSOR_HW_EVT(hdl, evt_type)                \
do {                                                \
    sensor_hdl_t *ss_hdl = (hdl)->hdl;\
    if (ss_hdl->hwcb) {                             \
        can5_sensor_hw_evt_t evt = {                \
            .type = (evt_type),                     \
            .sensor_id = get_sensor_id(hdl),        \
        };                                          \
        ss_hdl->hwcb(&evt);                         \
    }                                               \
} while (0)

/* ---------------------------------------------------------------------
 * Function prototypes
 -----------------------------------------------------------------------*/

static can5_err_t _read_bme(sensor_hdl_t *s_hdl, can5_sensor_bme280_data_t *bme280_data);

static can5_err_t _get_bme280_temperature(sensor_hdl_t *s_hdl, can5_sensor_bme280_data_t *bme280_data);

static can5_err_t _get_bme280_pressure(sensor_hdl_t *s_hdl, can5_sensor_bme280_data_t *bme280_data);

static can5_err_t _get_bme280_humidity(sensor_hdl_t *s_hdl, can5_sensor_bme280_data_t *bme280_data);


/* ---------------------------------------------------------------------
* Function definition
-----------------------------------------------------------------------*/

static can5_sensor_hdl_t *alloc(size_t *len)
{
     can5_sensor_hdl_t *hdl;
     sensor_hdl_t *s_hdl;
     *len = sizeof(can5_sensor_hdl_t) + sizeof(sensor_hdl_t);

     hdl = malloc(*len);
     if (!hdl) {
         return NULL;
     }

     s_hdl = hdl->hdl = hdl->hdl_data;

     CLEAR_STRUCT(*s_hdl);
     s_hdl->port = CAN5_PORT_NULL;
     s_hdl->sensor_id = CAN5_SENSOR_ID_NONE;
     s_hdl->status = SENSORDRIV_BME280_STAT_UNINITD;

     return hdl;
}

static uint8_t get_sensor_id(can5_sensor_hdl_t *hdl)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    return s_hdl->sensor_id;
}

static void set_sensor_id(can5_sensor_hdl_t *hdl, uint8_t sensor_id)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    s_hdl->sensor_id = sensor_id;
}

static can5_err_t detect(can5_sensor_hdl_t *hdl, can5_port_idx_t port)
{
    TRACE_FUNC;
    uint8_t chip_id;

    if (hal.i2c_read_reg(CAN5_BME280_SENSOR_ADDR, CAN5_BME280_REG_CHIP_ID, &chip_id, 1, I2C_MASTER_NACK) != CAN5_SUCCESS) {
        return CAN5_SENSOR_ERR_BME280_DETECT;
    }

    if (chip_id != 0x60){
        return CAN5_SENSOR_ERR_BME280_DETECT;
    }

    ESP_LOGI(TAG, "BME280 DETECT SUCCESS | CHIP ID : 0x%X", chip_id);

    return CAN5_SUCCESS;
}



static can5_err_t init(can5_sensor_hdl_t *hdl, can5_port_idx_t port, can5_sensor_hwcb_f *hwcb)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;
    uint8_t CALIB_DATA[CAN5_BME280_CALIB_DATA_LEN];

    if (s_hdl->port != port) {
        VERIFY_SUCCESS(detect(hdl, port));
    }


    VERIFY_SUCCESS(sensordriv_bme280.ops.driverctl(hdl, SENSOR_BME280_RESET, NULL, NULL));
    VERIFY_SUCCESS(sensordriv_bme280.ops.driverctl(hdl, SENSOR_BME280_FORCED_MODE, NULL, NULL));

    CLEAR_ARRAY(CALIB_DATA);
    //read compensation params
    VERIFY_SUCCESS(hal.i2c_read_reg(CAN5_BME280_SENSOR_ADDR, CAN5_BME280_REG_CALIB_00, &CALIB_DATA[0], (CAN5_BME280_CALIB_DATA_LEN-7), I2C_MASTER_LAST_NACK));
    VERIFY_SUCCESS(hal.i2c_read_reg(CAN5_BME280_SENSOR_ADDR, CAN5_BME280_REG_CALIB_26, &CALIB_DATA[25], 7, I2C_MASTER_LAST_NACK));

    // for (int i =0; i < CAN5_BME280_CALIB_DATA_LEN; i++){
    //     ESP_LOGI(TAG, "COMP_DATA [%d] : 0x%X", i, CALIB_DATA[i]);
    // }

    s_hdl->comp_data.dig_T1 = (uint16_t)((uint16_t)CALIB_DATA[0] | (uint16_t)CALIB_DATA[1] << 8);
    s_hdl->comp_data.dig_T2 = (int16_t)((uint16_t)CALIB_DATA[2] | (uint16_t)CALIB_DATA[3] << 8);
    s_hdl->comp_data.dig_T3 = (int16_t)((uint16_t)CALIB_DATA[4] | (uint16_t)CALIB_DATA[5] << 8);

    s_hdl->comp_data.dig_P1 = (uint16_t)((uint16_t)CALIB_DATA[6] | (uint16_t)CALIB_DATA[7] << 8);
    s_hdl->comp_data.dig_P2 = (int16_t)((uint16_t)CALIB_DATA[8] | (uint16_t)CALIB_DATA[9] << 8);
    s_hdl->comp_data.dig_P3 = (int16_t)((uint16_t)CALIB_DATA[10] | (uint16_t)CALIB_DATA[11] << 8);
    s_hdl->comp_data.dig_P4 = (int16_t)((uint16_t)CALIB_DATA[12] | (uint16_t)CALIB_DATA[13] << 8);
    s_hdl->comp_data.dig_P5 = (int16_t)((uint16_t)CALIB_DATA[14] | (uint16_t)CALIB_DATA[15] << 8);
    s_hdl->comp_data.dig_P6 = (int16_t)((uint16_t)CALIB_DATA[16] | (uint16_t)CALIB_DATA[17] << 8);
    s_hdl->comp_data.dig_P7 = (int16_t)((uint16_t)CALIB_DATA[18] | (uint16_t)CALIB_DATA[19] << 8);
    s_hdl->comp_data.dig_P8 = (int16_t)((uint16_t)CALIB_DATA[20] | (uint16_t)CALIB_DATA[21] << 8);
    s_hdl->comp_data.dig_P9 = (int16_t)((uint16_t)CALIB_DATA[22] | (uint16_t)CALIB_DATA[23] << 8);

    s_hdl->comp_data.dig_H1 = (unsigned char)((uint16_t)CALIB_DATA[24]);
    s_hdl->comp_data.dig_H2 = (int16_t)((uint16_t)CALIB_DATA[25] | (uint16_t)CALIB_DATA[26] << 8);
    s_hdl->comp_data.dig_H3 = (unsigned char)((uint16_t)CALIB_DATA[27]);
    s_hdl->comp_data.dig_H4 = (int16_t)((uint16_t)CALIB_DATA[28] << 4 | (uint16_t)(0x0F & CALIB_DATA[29]));
    s_hdl->comp_data.dig_H5 = (int16_t)((uint16_t)((0xF0 & CALIB_DATA[29]) >> 4) | (uint16_t)CALIB_DATA[30] << 4);
    s_hdl->comp_data.dig_H6 = (char)((uint16_t)CALIB_DATA[31]);

    s_hdl->hwcb = hwcb;
    s_hdl->status = SENSORDRIV_BME280_STAT_INITD;

    return CAN5_SUCCESS;
}

static can5_err_t uninit(can5_sensor_hdl_t *hdl)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    CHECK_INITD(s_hdl);

    s_hdl->port = CAN5_PORT_NULL;

    s_hdl->status = SENSORDRIV_BME280_STAT_UNINITD;

    return CAN5_SUCCESS;
}

static can5_err_t run(can5_sensor_hdl_t *hdl)
{
    //TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;
    can5_err_t ret;

    CHECK_INITD(s_hdl);

    if (s_hdl->async_read) {
        can5_sensor_bme280_data_t bme280_data_inst;
        can5_sensor_bme280_data_t *bme280_data;
        CLEAR_STRUCT(bme280_data_inst);

        ret = _read_bme(s_hdl, &bme280_data_inst);

        if (ret == CAN5_SUCCESS) {
            s_hdl->read.n++;
            s_hdl->read.data.humi +=
                    (bme280_data_inst.humi - s_hdl->read.data.humi) / s_hdl->read.n;
            s_hdl->read.data.pres +=
                    (bme280_data_inst.pres - s_hdl->read.data.pres) / s_hdl->read.n;
            s_hdl->read.data.temp +=
                    (bme280_data_inst.temp - s_hdl->read.data.temp) / s_hdl->read.n;
        }

        ESP_LOGI_V(TAG, "Read time %ld system_time %ld %s",  s_hdl->read_time_end, can5_time_ms(NULL), can5_err_to_str(ret));
        if (s_hdl->read_time_end <= can5_time_ms(NULL)) {
            s_hdl->async_read = false;
            s_hdl->status = SENSORDRIV_BME280_STAT_INITD;

            if (s_hdl->read.n > 0) {
                CLEAR_STRUCT(s_hdl->cb_data);
                bme280_data = &s_hdl->cb_data;
                *bme280_data = s_hdl->read.data;
                ESP_LOGI(TAG, "Averaged %d datapoints", s_hdl->read.n);

                s_hdl->readcb(get_sensor_id(hdl), bme280_data, sizeof(*bme280_data));
                SENSOR_HW_EVT(hdl, CAN5_SENSOR_HWEVT_READ_COMPLETE);
            }
            else {
                SENSOR_HW_EVT(hdl, CAN5_SENSOR_HWEVT_READ_FAILURE);
            }
        }

    }

    return CAN5_SUCCESS;
}

static can5_err_t _read_bme(sensor_hdl_t *s_hdl, can5_sensor_bme280_data_t *bme280_data)
{
  TRACE_FUNC;
  uint8_t ADC_DATA[CAN5_BME280_BURST_READ_LEN];

  bme280_data->humi = NAN;
  bme280_data->pres = NAN;
  bme280_data->temp = NAN;


  VERIFY_SUCCESS(hal.i2c_read_reg(CAN5_BME280_SENSOR_ADDR, CAN5_BME280_REG_ADC_BURST_READ_START,
                                             &ADC_DATA[0], CAN5_BME280_BURST_READ_LEN, I2C_MASTER_LAST_NACK));

  s_hdl->adc_data.adc_P = (int32_t)((uint32_t)ADC_DATA[0] << 12 | (uint32_t)ADC_DATA[1] << 4 | (uint32_t)ADC_DATA[2] >> 4);
  s_hdl->adc_data.adc_T = (int32_t)((uint32_t)ADC_DATA[3] << 12 | (uint32_t)ADC_DATA[4] << 4 | (uint32_t)ADC_DATA[5] >> 4);
  s_hdl->adc_data.adc_H = (int32_t)((uint32_t)ADC_DATA[6] << 8 | (uint32_t)ADC_DATA[7]);

  VERIFY_SUCCESS(_get_bme280_temperature(s_hdl, bme280_data));
  VERIFY_SUCCESS(_get_bme280_pressure(s_hdl, bme280_data));
  VERIFY_SUCCESS(_get_bme280_humidity(s_hdl, bme280_data));

  return ESP_OK;
}

static can5_err_t _get_bme280_temperature(sensor_hdl_t *s_hdl, can5_sensor_bme280_data_t *bme280_data){

    TRACE_FUNC;

    int32_t var1, var2, T;

    var1 = (((s_hdl->adc_data.adc_T>>3)-((int32_t)s_hdl->comp_data.dig_T1<<1)) * ((int32_t)s_hdl->comp_data.dig_T2)) >> 11;
    var2 = (((((s_hdl->adc_data.adc_T>>4) - ((int32_t)s_hdl->comp_data.dig_T1)) * ((s_hdl->adc_data.adc_T>>4) - ((int32_t)s_hdl->comp_data.dig_T1)))>> 12) *((int32_t)s_hdl->comp_data.dig_T3)) >> 14;

    s_hdl->t_fine = var1 + var2;

    T = (s_hdl->t_fine * 5 + 128) >> 8;

    bme280_data->temp = (float)T/100;

    //printf("Temperature : %.2f *C\n", bme280_data->temp);

    return CAN5_SUCCESS;
}

static can5_err_t _get_bme280_pressure(sensor_hdl_t *s_hdl, can5_sensor_bme280_data_t *bme280_data){

    TRACE_FUNC;

    int64_t var1, var2, p;
    float pres;

    var1 = ((int64_t)s_hdl->t_fine)-128000;
    var2 = var1 * var1 * (int64_t)s_hdl->comp_data.dig_P6;
    var2 = var2 + ((var1*(int64_t)s_hdl->comp_data.dig_P5)<<17);
    var2 = var2 + (((int64_t)s_hdl->comp_data.dig_P4)<<35);
    var1 = ((var1 * var1 * (int64_t)s_hdl->comp_data.dig_P3)>>8) + ((var1 * (int64_t)s_hdl->comp_data.dig_P2)<<12);
    var1 = (((((int64_t)1)<<47)+var1))*((int64_t)s_hdl->comp_data.dig_P1)>>33;

    if (var1 == 0){
        TRACE_FUNC;
        return 0; // avoid exception caused by division by zero
    }

    p = 1048576-s_hdl->adc_data.adc_P;
    p = (((p<<31)-var2)*3125)/var1;
    var1 = (((int64_t)s_hdl->comp_data.dig_P9) * (p>>13) * (p>>13)) >> 25;
    var2 = (((int64_t)s_hdl->comp_data.dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)s_hdl->comp_data.dig_P7)<<4);
    p = (uint32_t)p;
    pres = ((float)p/256)/100; //  /256 returns p in Pascals & /100 retuns p in hectoPascal (hPa)


    bme280_data->pres = pres;
    //printf("Pressure : %.2f hPa\n", bme280_data->pres);

    return CAN5_SUCCESS;
}

static can5_err_t _get_bme280_humidity(sensor_hdl_t *s_hdl, can5_sensor_bme280_data_t *bme280_data){

    TRACE_FUNC;

    // printf("adc_H : %X\n", s_hdl->adc_data.adc_H);
    // printf("dig_H1 : %X\n", s_hdl->comp_data.dig_H1);
    // printf("dig_H2 : %X\n", s_hdl->comp_data.dig_H2);
    // printf("dig_H3 : %X\n", s_hdl->comp_data.dig_H3);
    // printf("dig_H4 : %X\n", s_hdl->comp_data.dig_H4);
    // printf("dig_H5 : %X\n", s_hdl->comp_data.dig_H5);
    // printf("dig_H6 : %X\n", s_hdl->comp_data.dig_H6);

    int32_t v_x1_u32r;
    float humi;

    v_x1_u32r = (s_hdl->t_fine - ((int32_t)76800));
    v_x1_u32r = (((((s_hdl->adc_data.adc_H << 14) - (((int32_t)s_hdl->comp_data.dig_H4) << 20) - (((int32_t)s_hdl->comp_data.dig_H5)*v_x1_u32r)) +
                ((int32_t)16384)) >> 15) *
                (((((((v_x1_u32r *((int32_t)s_hdl->comp_data.dig_H6)) >> 10) * (((v_x1_u32r * ((int32_t)s_hdl->comp_data.dig_H3)) >> 11) +
                ((int32_t)32768))) >> 10) + ((int32_t)2097152)) * ((int32_t)s_hdl->comp_data.dig_H2) + 8192) >> 14));
    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)s_hdl->comp_data.dig_H1)) >> 4));
    v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
    v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);

    v_x1_u32r = (uint32_t)(v_x1_u32r>>12);

    humi = (float)v_x1_u32r/1024;

    bme280_data->humi = humi;

    // printf("Humidity : %.2f %%\n", bme280_data->humi);

    return CAN5_SUCCESS;
}


/**
 * @brief Read sensor data
 * @param prxdata  buffer to read data into, can be null if blocking is false
 * @param plen  size to write to, can be null if blocking is false
 * @param blocking if this function should block or dispatch read
 */
static can5_err_t read(can5_sensor_hdl_t *hdl, void *prxdata, size_t *plen, time_t read_time_end, bool blocking)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    CHECK_INITD(s_hdl);
    if (s_hdl->status == SENSORDRIV_BME280_STAT_READING) {
        return CAN5_SENSOR_ERR_BUSY;
    }

    s_hdl->status = SENSORDRIV_BME280_STAT_READING;

    CLEAR_STRUCT(s_hdl->read.data);
    s_hdl->read.n = 0;

    s_hdl->read_time_end = read_time_end;

    VERIFY_SUCCESS(sensordriv_bme280.ops.driverctl(hdl, SENSOR_BME280_FORCED_MODE, NULL, NULL));

    if (blocking) {
        SENSOR_HW_EVT(hdl, CAN5_SENSOR_HWEVT_UNIMPLIMENTED);
        s_hdl->status = SENSORDRIV_BME280_STAT_INITD;
        return CAN5_ERR_UNIMPLEMENTED;
    } else {
        // dispatch the read to run() and call read_cb when done
        s_hdl->async_read = true;
    }
    return CAN5_SUCCESS;
}



static can5_err_t register_read_cb(can5_sensor_hdl_t *hdl, can5_sensor_readcb_f* cb)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    VERIFY_NOT_NULL(cb);
    s_hdl->readcb = cb;

    return CAN5_SUCCESS;
}


static can5_err_t enable(can5_sensor_hdl_t *hdl, bool enable)
{
    /* not applicable */
    return CAN5_SUCCESS;
}

static can5_sensor_data_list_t *get_sensor_data(can5_sensor_hdl_t *hdl, const void *data, size_t data_len)
{
    sensor_hdl_t *s_hdl = hdl->hdl;
    const can5_sensor_bme280_data_t *s_data = data;
    can5_sensor_data_list_t *list;
    can5_sensor_data_t *elem;

    list = NULL;

    if (!s_data) {
        goto done;
    }

    if (sizeof(can5_sensor_bme280_data_t) != data_len) {
        goto done;
    }

    list = calloc(sizeof (can5_sensor_data_list_t), 1);
    if (!list) {
        goto done;
    }

    TAILQ_INIT(list);

    elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_BME280_TEMP, s_hdl->port,
                                    CAN5_SENSOR_DATA_DATATYPE_DEC, NULL, 0, s_data->temp);
    if (elem) {
        TAILQ_INSERT_TAIL(list, elem, te);
    }

    elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_BME280_HUMI, s_hdl->port,
                                    CAN5_SENSOR_DATA_DATATYPE_DEC, NULL, 0, s_data->humi);
    if (elem) {
        TAILQ_INSERT_TAIL(list, elem, te);
    }

    elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_BME280_PRES, s_hdl->port,
                                    CAN5_SENSOR_DATA_DATATYPE_DEC, NULL, 0, s_data->pres);
    if (elem) {
        TAILQ_INSERT_TAIL(list, elem, te);
    }

done:
    return list;

}

static bool is_running(can5_sensor_hdl_t *hdl)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    return s_hdl->status >= SENSORDRIV_BME280_STAT_INITD;
}

static can5_err_t driverctl(can5_sensor_hdl_t *hdl, uint8_t request,const void *params, void *response)
{
    TRACE_FUNC;

    //CHECK_INITD();
    uint8_t reset = 0xB6; //0b10110110
    uint8_t ctrl_hum = 0b00000001; //set osrs_h[2:0] oversampling x 1 | XXXXX,osrs_h = 0bXXXXX,001
    uint8_t ctrl_meas_sleep = 0b00100100; //set osrs_t[2:0] | oversampling x 1, osrs_p[2:0] | oversampling x 1, mode[1:0] | sleep || 0b001,001,00
    uint8_t ctrl_meas_forced = 0b00100101; //set osrs_t[2:0] | oversampling x 1, osrs_p[2:0] | oversampling x 1, mode[1:0] | forced || 0b001,001,01
    uint8_t ctrl_meas_normal = 0b01010111; //set osrs_t[2:0] | oversampling x 2, osrs_p[2:0] | oversampling x 16, mode[1:0] | normal || 0b010,101,11
    uint8_t config = 0b00010000; // set t_sb[2:0] | 0.5 ms, filter[2:0] | 16, R | X, spi3w_en[0] | 0 || 0b000,100,00

    switch (request) {
        case SENSOR_BME280_SLEEP_MODE:
            //set humidity, temperature, pressure acquisition options and sensor mode
            VERIFY_SUCCESS(hal.i2c_write_reg(CAN5_BME280_SENSOR_ADDR, CAN5_BME280_REG_CTRL_HUM, &ctrl_hum, 1, true));
            VERIFY_SUCCESS(hal.i2c_write_reg(CAN5_BME280_SENSOR_ADDR, CAN5_BME280_REG_CTRL_MEAS, &ctrl_meas_sleep, 1, true));
            break;

        case SENSOR_BME280_FORCED_MODE:
            //set humidity, temperature, pressure acquisition options and sensor mode
            VERIFY_SUCCESS(hal.i2c_write_reg(CAN5_BME280_SENSOR_ADDR, CAN5_BME280_REG_CTRL_HUM, &ctrl_hum, 1, I2C_MASTER_NACK));
            VERIFY_SUCCESS(hal.i2c_write_reg(CAN5_BME280_SENSOR_ADDR, CAN5_BME280_REG_CTRL_MEAS, &ctrl_meas_forced, 1, true));
            break;

        case SENSOR_BME280_NORMAL_MODE:
            //set humidity, temperature, pressure acquisition options and sensor mode
            VERIFY_SUCCESS(hal.i2c_write_reg(CAN5_BME280_SENSOR_ADDR, CAN5_BME280_REG_CTRL_HUM, &ctrl_hum, 1, I2C_MASTER_NACK));
            VERIFY_SUCCESS(hal.i2c_write_reg(CAN5_BME280_SENSOR_ADDR, CAN5_BME280_REG_CTRL_MEAS, &ctrl_meas_normal, 1, true));
            //set standby time and filter coefficient for normal mode
            VERIFY_SUCCESS(hal.i2c_write_reg(CAN5_BME280_SENSOR_ADDR, CAN5_BME280_REG_CONFIG, &config, 1, true));
            break;

        case SENSOR_BME280_RESET:
            VERIFY_SUCCESS(hal.i2c_write_reg(CAN5_BME280_SENSOR_ADDR, CAN5_BME280_REG_RESET, &reset, 1, true));
            break;

        default:
            break;
    }

    /* Let the mod set register */
    vTaskDelay(1);

    return CAN5_SUCCESS;
}

static int32_t status_get(can5_sensor_hdl_t *hdl)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    return s_hdl->status;
}
//_______________________________________________________________________________________________________
//
//   DEBUG SUPPORT
//-------------------------------------------------------------------------------------------------------


#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL > 0

/**
 * @brief NEtif Status tags
 *
 */
static const can5_tag_tab_t _SENSORDRIV_BME280_STAT_tags = {
        TAG_TAB_ITEM(SENSORDRIV_BME280_STAT_UNINITD),
        TAG_TAB_ITEM(SENSORDRIV_BME280_STAT_INITD),
        TAG_TAB_ITEM(SENSORDRIV_BME280_STAT_READING),
};


static const char *status_getstr(can5_sensor_hdl_t *hdl, int32_t status)
{
    TRACE_FUNC;

    return TAG_LOOKUP(status, _SENSORDRIV_BME280_STAT_tags);
}

#endif

void parse_bme280_read_data(const can5_sensordriv_t *driv, const can5_sensor_bme280_data_t *bme280_data,
                            void *pout, size_t *plen)
{
    TRACE_FUNC;
    //if (!co2_data->read) return;

    memset(pout, 0, CAN5_SENSOR_PARSED_DATA_MAX_LEN);
    snprintf(pout, CAN5_SENSOR_PARSED_DATA_MAX_LEN, "%s:%.4f,%s:%.4f,%s:%.4f",
             can5_hazemon_get_type(HAZEMON_TEMPERATURE)->token, bme280_data->temp,
             can5_hazemon_get_type(HAZEMON_PRESSURE)->token, bme280_data->pres,
             can5_hazemon_get_type(HAZEMON_HUMIDITY)->token, bme280_data->humi);
    *plen = strlen(pout);
}
