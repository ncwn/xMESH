/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   29/05/23
*
* File:  can5_sensor_mpu6500_imu.c
* Descr:
*******************************************************************************/

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>
#include <endian.h>
#include "can5_hazemon_types.h"
#include "esp_log.h"
#include "can5_error.h"
#include "can5_utils.h"
#include "can5_hal.h"
#include "can5_sensor_mpu6500.h"


static const char *TAG = "SENSOR_MPU6500";

/*
 * https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6500-Register-Map2.pdf
 */

#define MPU6500_SENSOR_ADDR                 0x68

#define MPU6500_GYRO_CONFIG                 0x1B
#define MPU6500_ACCEL_CONFIG                0x1C


#define MPU6500_SENSOR_DATA                 0x3B
typedef struct sensor_data_reg_s {
    int16_t accel_xout;
    int16_t accel_yout;
    int16_t accel_zout;
    int16_t temp_out;
    int16_t gyro_xout;
    int16_t gyro_yout;
    int16_t gyro_zout;
} __attribute__((packed)) sensor_data_reg_t;

#define MPU6500_USER_CTRL                   0x6A
#define MPU6500_USER_CTRL_DATA              0b10001101
#define MPU6500_PWR_MGMT_1                  0x6B
#define MPU6500_PWR_MGMT_1_DATA             0x0
#define MPU6500_REG_CHIP_ID                 0x75


#define CHECK_INITD(s_hdl) do {                       \
    if ((s_hdl)->status == SENSORDRIV_MPU6500_STAT_UNINITD) return CAN5_ERR_INVALID_STATE; \
} while(0)

#if 0
#define TRACE_FUNC ESP_LOGI(TAG, "in -> %s() :%d", __FUNCTION__, __LINE__)
#else
#define TRACE_FUNC
#endif

typedef enum sensordriv_mpu6500_status_e {
    SENSORDRIV_MPU6500_STAT_UNINITD = 0,
    SENSORDRIV_MPU6500_STAT_INITD,
    SENSORDRIV_MPU6500_STAT_READING,
} sensordriv_mpu6500_status_t;

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


const can5_sensordriv_t sensordriv_mpu6500 = {
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
            .type = CAN5_SENSORDRIV_TYPE_MPU6500,
        },
        .io_type = CAN5_PHY_IO_TYPE_I2C,
        .name = "MPU6500",
        .version = "1.0",
        .manufacturer = "InvenSense",
    }
};

typedef struct sensor_hdl_s {
    uint8_t sensor_id;
    volatile sensordriv_mpu6500_status_t  status;
    volatile bool async_read;
    can5_port_idx_t port;
    can5_sensor_readcb_f *readcb;
    can5_sensor_mpu6500_data_t cb_data;
    can5_sensor_hwcb_f  *hwcb;
    struct {
        float gyro;
        float accel;
    } resolution;

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
static can5_err_t __read_imu(sensor_hdl_t *s_hdl, can5_sensor_mpu6500_data_t *data);

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
     s_hdl->status = SENSORDRIV_MPU6500_STAT_UNINITD;

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
    can5_err_t ret;

    ret = hal.i2c_read_reg(MPU6500_SENSOR_ADDR, MPU6500_REG_CHIP_ID, &chip_id, 1, I2C_MASTER_LAST_NACK);

    if (ret != CAN5_SUCCESS) {
        return CAN5_SENSOR_ERR_INVALID_PORT;
    }

    if (chip_id != 0x70 && chip_id != 0x68){
        return CAN5_SENSOR_ERR_INVALID_PORT;
    }

    ESP_LOGI(TAG, "MPU6500 DETECT SUCCESS | CHIP ID : 0x%X", chip_id);

    return CAN5_SUCCESS;
}


#define RESOLUTION 32768.f

static can5_err_t init(can5_sensor_hdl_t *hdl, can5_port_idx_t port, can5_sensor_hwcb_f *hwcb)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;
    uint8_t byte;

    if (s_hdl->port != port) {
        VERIFY_SUCCESS(detect(hdl, port));
    }

    byte = MPU6500_PWR_MGMT_1_DATA;
    VERIFY_SUCCESS(hal.i2c_write_reg(MPU6500_SENSOR_ADDR, MPU6500_PWR_MGMT_1, (uint8_t *) &byte, 1, false));


    byte = MPU6500_USER_CTRL_DATA;
    VERIFY_SUCCESS(hal.i2c_write_reg(MPU6500_SENSOR_ADDR, MPU6500_USER_CTRL, (uint8_t *) &byte, 1, false));

    VERIFY_SUCCESS(hal.i2c_read_reg(MPU6500_SENSOR_ADDR, MPU6500_GYRO_CONFIG, &byte, 1, I2C_MASTER_LAST_NACK));

    byte = (byte >> 3) & 0b11;

    switch (byte) {
        case 0b00:
            s_hdl->resolution.gyro = 250.f / RESOLUTION;
            break;

        case 0b01:
            s_hdl->resolution.gyro = 500.f / RESOLUTION;
            break;

        case 0b10:
            s_hdl->resolution.gyro = 1000.f/ RESOLUTION;
            break;

        case 0b11:
            s_hdl->resolution.gyro = 2000.f/ RESOLUTION;
            break;

        default:
            return CAN5_SENSOR_ERR_TIMEOUT;
    }


    VERIFY_SUCCESS(hal.i2c_read_reg(MPU6500_SENSOR_ADDR, MPU6500_ACCEL_CONFIG, &byte, 1, I2C_MASTER_LAST_NACK));

    byte = (byte >> 3) & 0b11;

    switch (byte) {
        case 0b00:
            s_hdl->resolution.accel = 2.f / RESOLUTION;
            break;

        case 0b01:
            s_hdl->resolution.accel = 4.f/ RESOLUTION;
            break;

        case 0b10:
            s_hdl->resolution.accel = 8.f/ RESOLUTION;
            break;

        case 0b11:
            s_hdl->resolution.accel = 16.f/ RESOLUTION;
            break;

        default:
            return CAN5_SENSOR_ERR_TIMEOUT;
    }

    ESP_LOGI(TAG, "Gyro Resolution %f, Accel Resolution %f", s_hdl->resolution.gyro, s_hdl->resolution.accel);

    s_hdl->hwcb = hwcb;
    s_hdl->status = SENSORDRIV_MPU6500_STAT_INITD;
    
    return CAN5_SUCCESS;
}

static can5_err_t uninit(can5_sensor_hdl_t *hdl)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    CHECK_INITD(s_hdl);

    s_hdl->port = CAN5_PORT_NULL;

    s_hdl->status = SENSORDRIV_MPU6500_STAT_UNINITD;

    return CAN5_SUCCESS;
}

static can5_err_t run(can5_sensor_hdl_t *hdl)
{
    //TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    CHECK_INITD(s_hdl);

    if (s_hdl->async_read) {
        can5_sensor_mpu6500_data_t *mpu6500_data;
        mpu6500_data = &s_hdl->cb_data;
        CLEAR_STRUCT(s_hdl->cb_data);



        VERIFY_SUCCESS_SAFERETURN(__read_imu(s_hdl, mpu6500_data),
                                  {
                                      s_hdl->async_read = false;
                                      s_hdl->status = SENSORDRIV_MPU6500_STAT_INITD;
                                      SENSOR_HW_EVT(hdl, CAN5_SENSOR_HWEVT_READ_FAILURE);
                                  });


        s_hdl->async_read = false;
        s_hdl->status = SENSORDRIV_MPU6500_STAT_INITD;

        s_hdl->readcb(get_sensor_id(hdl), mpu6500_data, sizeof(*mpu6500_data));
        SENSOR_HW_EVT(hdl, CAN5_SENSOR_HWEVT_READ_COMPLETE);
    }

    return CAN5_SUCCESS;
}


/**
 * @brief Read sensor data
 * @param prxdata  buffer to read data into, can be null if blocking is false
 * @param plen  size to write to, can be null if blocking is false
 * @param blocking if this function should block or dispatch read
 */
static can5_err_t read(can5_sensor_hdl_t *hdl, void *prxdata, size_t *plen, time_t timeout, bool blocking)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    s_hdl->status = SENSORDRIV_MPU6500_STAT_READING;
    
    // uint8_t ADC_DATA[CAN5_MPU6500_BURST_READ_LEN];
    
    // VERIFY_SUCCESS(hal.i2c_read_reg(CAN5_MPU6500_SENSOR_ADDR, CAN5_MPU6500_REG_ADC_BURST_READ_START, &ADC_DATA[0], CAN5_MPU6500_BURST_READ_LEN, I2C_MASTER_LAST_NACK));
    
    // s_hdl->adc_data.adc_P = (int32_t)((uint32_t)ADC_DATA[0] << 12 | (uint32_t)ADC_DATA[1] << 4 | (uint32_t)ADC_DATA[2] >> 4);
    // s_hdl->adc_data.adc_T = (int32_t)((uint32_t)ADC_DATA[3] << 12 | (uint32_t)ADC_DATA[4] << 4 | (uint32_t)ADC_DATA[5] >> 4);
    // s_hdl->adc_data.adc_H = (int32_t)((uint32_t)ADC_DATA[6] << 8 | (uint32_t)ADC_DATA[7]);

    VERIFY_SUCCESS(sensordriv_mpu6500.ops.driverctl(hdl, SENSOR_MPU6500_FORCED_MODE, NULL, NULL));

    if (blocking) {
        can5_sensor_mpu6500_data_t mpu6500_data;
        VERIFY_SUCCESS_SAFERETURN(__read_imu(s_hdl, &mpu6500_data),
                                  {
                                      s_hdl->status = SENSORDRIV_MPU6500_STAT_INITD;
                                      SENSOR_HW_EVT(hdl, CAN5_SENSOR_HWEVT_READ_FAILURE);
                                  });
        memcpy(prxdata, &mpu6500_data, sizeof(mpu6500_data));
        if (plen) {
            *plen = sizeof(mpu6500_data);
        }
        SENSOR_HW_EVT(hdl, CAN5_SENSOR_HWEVT_READ_COMPLETE);
        s_hdl->status = SENSORDRIV_MPU6500_STAT_INITD;
        return CAN5_SUCCESS;
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
    const can5_sensor_mpu6500_data_t *s_data = data;
    can5_sensor_data_list_t *list;
    can5_sensor_data_t *elem;

    list = NULL;

    if (!s_data) {
        goto done;
    }

    if (sizeof(can5_sensor_mpu6500_data_t) != data_len) {
        goto done;
    }

    list = calloc(sizeof (can5_sensor_data_list_t), 1);
    if (!list) {
        goto done;
    }

    TAILQ_INIT(list);



    elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_MPU6400_ACCEL_X, s_hdl->port,
                                    CAN5_SENSOR_DATA_DATATYPE_DEC, NULL, 0, s_data->accel_x);
    if (elem) {
        TAILQ_INSERT_TAIL(list, elem, te);
    }

    elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_MPU6400_ACCEL_Y, s_hdl->port,
                                    CAN5_SENSOR_DATA_DATATYPE_DEC, NULL, 0, s_data->accel_y);
    if (elem) {
        TAILQ_INSERT_TAIL(list, elem, te);
    }

    elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_MPU6400_ACCEL_Z, s_hdl->port,
                                    CAN5_SENSOR_DATA_DATATYPE_DEC, NULL, 0, s_data->accel_z);
    if (elem) {
        TAILQ_INSERT_TAIL(list, elem, te);
    }



    elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_MPU6400_GYRO_X, s_hdl->port,
                                    CAN5_SENSOR_DATA_DATATYPE_DEC, NULL, 0, s_data->gyro_x);
    if (elem) {
        TAILQ_INSERT_TAIL(list, elem, te);
    }

    elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_MPU6400_GYRO_Y, s_hdl->port,
                                    CAN5_SENSOR_DATA_DATATYPE_DEC, NULL, 0, s_data->gyro_y);
    if (elem) {
        TAILQ_INSERT_TAIL(list, elem, te);
    }

    elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_MPU6400_GYRO_Z, s_hdl->port,
                                    CAN5_SENSOR_DATA_DATATYPE_DEC, NULL, 0, s_data->gyro_z);
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

    return s_hdl->status >= SENSORDRIV_MPU6500_STAT_INITD;
}

static can5_err_t driverctl(can5_sensor_hdl_t *hdl, uint8_t request,const void *params, void *response)
{
    TRACE_FUNC;

    switch (request) {
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


static can5_err_t __read_imu(sensor_hdl_t *s_hdl, can5_sensor_mpu6500_data_t *data)
{
    sensor_data_reg_t reg_data;

    VERIFY_SUCCESS(hal.i2c_read_reg(MPU6500_SENSOR_ADDR, MPU6500_SENSOR_DATA,
                                               (uint8_t *)&reg_data, sizeof(sensor_data_reg_t), I2C_MASTER_LAST_NACK));

    reg_data.accel_xout = be16toh(reg_data.accel_xout);
    reg_data.accel_yout = be16toh(reg_data.accel_yout);
    reg_data.accel_zout = be16toh(reg_data.accel_zout);

    reg_data.temp_out = be16toh(reg_data.temp_out);

    reg_data.gyro_xout = be16toh(reg_data.gyro_xout);
    reg_data.gyro_yout = be16toh(reg_data.gyro_yout);
    reg_data.gyro_zout = be16toh(reg_data.gyro_zout);

    data->accel_x = reg_data.accel_xout * s_hdl->resolution.accel;
    data->accel_y = reg_data.accel_yout * s_hdl->resolution.accel;
    data->accel_z = reg_data.accel_zout * s_hdl->resolution.accel;

    data->gyro_x = reg_data.gyro_xout * s_hdl->resolution.gyro * M_PI/180;
    data->gyro_y = reg_data.gyro_yout * s_hdl->resolution.gyro * M_PI/180;
    data->gyro_z = reg_data.gyro_zout * s_hdl->resolution.gyro * M_PI/180;

    data->imu_temp = reg_data.temp_out;

    //ESP_LOG_BUFFER_HEXDUMP(TAG, &data, sizeof(sensor_data_reg_t), ESP_LOG_INFO);

    return CAN5_SUCCESS;

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
static const can5_tag_tab_t _SENSORDRIV_MPU6500_STAT_tags = {
        TAG_TAB_ITEM(SENSORDRIV_MPU6500_STAT_UNINITD),
        TAG_TAB_ITEM(SENSORDRIV_MPU6500_STAT_INITD),
        TAG_TAB_ITEM(SENSORDRIV_MPU6500_STAT_READING),
};


static const char *status_getstr(can5_sensor_hdl_t *hdl, int32_t status)
{
    TRACE_FUNC;

    return TAG_LOOKUP(status, _SENSORDRIV_MPU6500_STAT_tags);
}

#endif

void parse_mpu6500_read_data(const can5_sensordriv_t *driv, const can5_sensor_mpu6500_data_t *mpu_6500_data,
                            void *pout, size_t *plen)
{
    TRACE_FUNC;
    //if (!co2_data->read) return;

    memset(pout, 0, CAN5_SENSOR_PARSED_DATA_MAX_LEN);
    snprintf(pout, CAN5_SENSOR_PARSED_DATA_MAX_LEN, "%s:%.4f,%s:%.4f,%s:%.4f,%s:%.4f,%s:%.4f,%s:%.4f",
             "A_x", mpu_6500_data->accel_x,
             "A_y", mpu_6500_data->accel_y,
             "A_z", mpu_6500_data->accel_z,
             "G_x", mpu_6500_data->gyro_x,
             "G_y", mpu_6500_data->gyro_y,
             "G_z", mpu_6500_data->gyro_z);
    *plen = strlen(pout);
}


int64_t parse_mpu6500_to_csv(const can5_sensor_mpu6500_data_t *mpu_6500_data, int64_t timestamp_ms, char *pout)
{
    TRACE_FUNC;
    return snprintf(pout, CAN5_SENSOR_PARSED_DATA_MAX_LEN, "%lld,%.8f,%.8f,%.8f,%.8f,%.8f,%.8f",
             timestamp_ms,
             mpu_6500_data->accel_x,
              mpu_6500_data->accel_y,
              mpu_6500_data->accel_z,
              mpu_6500_data->gyro_x,
              mpu_6500_data->gyro_y,
              mpu_6500_data->gyro_z);
}
