/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   12/01/22
*
* File:  can5_sensormng.c
* Descr:
*******************************************************************************/
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <can5_hal.h>
#include <can5_config_provider.h>
#include <can5_sensor_ublox_neo_gps.h>
#include <can5_types.h>
#include <can5_storagemng.h>
#include <can5_sensor_ze07_co_adc.h>
#include <can5_sensor_mh_z16_co2.h>
#include <can5_events.h>
#include "can5_hazemon_types.h"
#include <can5_sensor_ze03_no2.h>
#include <can5_sensor_bme280.h>
#include <can5_sensor_pms7003.h>
#include "esp_log.h"
#include "can5_sensormng.h"
#include "can5_error.h"
#include "can5_utils.h"
#include "can5_sensordriv.h"
#include "can5_sensor_ze07_co.h"
#include "can5_sensor_sim7600_gps.h"
#include "can5_actuatordriv.h"
#include "can5_actuator_relay.h"
#include "can5_sensor_ws3226.h"
#include "can5_sensor_mpu6500.h"
#include "can5_sensor_scd41.h"

static const char *TAG = "SENSORMNG";

#if 0
#define TRACE_FUNC ESP_LOGI(TAG, "in -> %s() :%d", __FUNCTION__, __LINE__)
#else
#define TRACE_FUNC
#endif

#define DETECT_INTERVAL     60  // 20 seconds

typedef enum sensormng_stat_e {
    SENSORMNG_STAT_UNINITD = 0,
    SENSORMNG_STAT_INITD,
} sensormng_stat_t;

static can5_err_t init();

static can5_err_t uninit();

static can5_err_t run();

static can5_err_t pre_sleep();

static can5_err_t post_sleep();

static bool is_sleepable();

static bool is_sleeping();

static int32_t status_get();

#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL > 0

static const char *status_getstr(int32_t status);

static const char *evt_getstr(int32_t evt);

#endif

static can5_err_t enable_sensor(bool enable, uint8_t sensor_id);

static time_t read_time(void);

static time_t warm_up_time(void);

static can5_err_t warm_up_time_array(can5_sensormng_wake_up_time_t *wake_up, size_t *len);

static can5_err_t get_ready_sensors(can5_sensor_meta_details_t *sensors_meta, size_t *len);

static can5_err_t run_sensor_commands(const can5_sensordriv_type_t sensor_type, const int cmd, const void *params, void *response);


const can5_sensormng_t sensor_manager = {
    .module = {
        .init = init,
        .uninit = uninit,
        .run = run,
        .pre_sleep = pre_sleep,
        .post_sleep = post_sleep,
        .is_sleepable = is_sleepable,
        .is_sleeping = is_sleeping,
        .status_get = status_get,
        .status_getstr = status_getstr,
        .evt_getstr = evt_getstr,
    },
    .enable_sensor = enable_sensor,
    .read_time = read_time,
    .warm_up_time = warm_up_time,
    .warm_up_time_array = warm_up_time_array,
    .get_ready_sensors = get_ready_sensors,
    .run_sensor_commands = run_sensor_commands,
};

#define ACT_MAP(c, t)   { .cmd = (c), .type = (t) }

const static struct {
    can5_actuator_cmd_t cmd;
    can5_actuatordriv_type_t type;
} __actuator_map[] = {
    ACT_MAP(CAN5_ACTUATOR_CMD_RELAY_LOW, CAN5_ACTUATORDRIV_TYPE_RELAY),
    ACT_MAP(CAN5_ACTUATOR_CMD_RELAY_HIGH, CAN5_ACTUATORDRIV_TYPE_RELAY),
};

typedef struct io_ports_s {
    can5_port_idx_t port;
    bool is_sensor;
    bool is_actuator;
    union {
        can5_sensordriv_type_t sensor;
        can5_actuatordriv_type_t actuator;
    };
} io_port_t;

typedef struct actuator_inst_s {
    uint8_t id;
    can5_actuatordriv_t const *actuatordriv;
    can5_actuator_hdl_t *hdl;
    can5_port_idx_t port;
    TAILQ_ENTRY(actuator_inst_s) te;         // member for lists
} actuator_inst_t;

typedef TAILQ_HEAD(actuator_inst_list_head_s, actuator_inst_s) actuator_inst_list_head_t;

typedef struct actuator_inst_list_s {
    actuator_inst_list_head_t undetected;   // actuator undetected
    actuator_inst_list_head_t detected;     // detected reading
    SemaphoreHandle_t mutex;
} actuator_inst_list_t;

typedef struct sensor_inst_s {
    uint8_t sensor_id;
    can5_sensordriv_t const *sensordriv;
    can5_sensor_hdl_t *hdl;
    can5_port_idx_t port;
    time_t last_detect_time;
    bool is_realtime;
    char last_reading[CAN5_SENSOR_PARSED_DATA_MAX_LEN];
    TAILQ_ENTRY(sensor_inst_s) te;         // member for lists
} sensor_inst_t;

typedef TAILQ_HEAD(sensor_inst_list_head_s, sensor_inst_s) sensor_inst_list_head_t;

typedef struct task_s {
    TaskHandle_t hdl;
    StaticTask_t buffer;
} task_t;

static struct {
    StackType_t sensormng[CONFIG_CAN5_SENSORMNG_TASK_STACK_SIZE];
    StackType_t i2c[CONFIG_CAN5_SENSORMNG_I2C_TASK_STACK_SIZE];
#if CONFIG_CAN5_SENSORMNG_ADC_TASK_ENABLE
    StackType_t adc[CONFIG_CAN5_SENSORMNG_ADC_TASK_STACK_SIZE];
#endif
    StackType_t uart[CONFIG_CAN5_SENSORMNG_UART_TASK_STACK_SIZE];
#if CONFIG_CAN5_HIGH_FREQ_IMU_ENABLE
    StackType_t high_freq[CONFIG_CAN5_SENSORMNG_HIGH_FREQ_IMU_TASK_STACK_SIZE];
#endif
} task_stacks;

typedef struct sensor_inst_list_s {
    can5_phy_io_type_t type;
    sensor_inst_list_head_t undetected;     // sensors undetected
    sensor_inst_list_head_t ready;          // sensors reading
    sensor_inst_list_head_t running;        // sensors running
    SemaphoreHandle_t mutex;
    task_t task;
} sensor_inst_list_t;

typedef struct scheduled_cycle_data_s {
    can5_sensor_data_list_t data_list;
    SemaphoreHandle_t mutex;
    time_t timestamp;                      // in seconds
    bool gps_set;
} scheduled_cycle_data_t;

typedef struct allocated_params_s {
    bool is_high_freq_imu;
    bool is_realtime;
    sensor_inst_t *out_imu_sensor_inst;
} allocate_params_t;

static struct sensormng_hdl {
    volatile sensormng_stat_t status;
    can5_sensordriv_t const *sensor_driv[CAN5_SENSORDRIV_TYPE_COUNT];
    can5_actuatordriv_t const *actuator_driv[CAN5_ACTUATORDRIV_TYPE_COUNT + 1];
    sensor_inst_list_t sensor_list[CAN5_PHY_IO_TYPE_COUNT];
    actuator_inst_list_t actuator_list;
    task_t sensormng_task;
    EventGroupHandle_t evt_grp;
    scheduled_cycle_data_t scheduled_cycle_data;
    esp_timer_handle_t timer;
#if CONFIG_CAN5_HIGH_FREQ_IMU_ENABLE
    struct {
        task_t task;
        uint8_t buffer[CONFIG_CAN5_HIGH_FREQ_IMU_BUFFER_SIZE];
        size_t buffer_len;
    } high_freq;
#endif
} __sensormng = {
    .status = SENSORMNG_STAT_UNINITD,
    .sensor_driv = {
        &sensordriv_ublox_neo,
        &sensordriv_ze07_co_adc,
        &sensordriv_mh_z16_co2,
        &sensordriv_ze03_no2,
        &sensordriv_ze07_co,
        &sensordriv_bme280,
        &sensordriv_pms7003,
        &sensordriv_sim7600_gps,
        &sensordriv_ws3226,
        &sensordriv_mpu6500,
        &sensordriv_scd41,
        NULL,
    },
    .actuator_driv = {
        &actuatordriv_relay,
        NULL,
    },
};

#if CONFIG_CAN5_SENSORMNG_ADC_TASK_ENABLE
#define S_ADC                           (&__sensormng.sensor_list[CAN5_PHY_IO_TYPE_ADC])
#endif
#define S_I2C                           (&__sensormng.sensor_list[CAN5_PHY_IO_TYPE_I2C])
#define S_UART                          (&__sensormng.sensor_list[CAN5_PHY_IO_TYPE_UART])
#define S_TYPE(type)                    (&__sensormng.sensor_list[type])


#define S_MUTEX(mutex, stmts)           if (xSemaphoreTake(mutex, portMAX_DELAY)) {     \
                                            stmts;                                      \
                                            xSemaphoreGive(mutex);                      \
                                        }

#define S_MUTEX_TYPE(type, stmts)       S_MUTEX(S_TYPE(type)->mutex, stmts)

#define S_LIST_MAX(type, list, member, max)  {                                          \
    sensor_inst_t  *cur;                                                                \
    if ((max) == NULL) {                                                                \
        (max) = TAILQ_FIRST(&S_TYPE(type)->list);                                       \
    }                                                                                   \
    TAILQ_FOREACH(cur, &S_TYPE(type)->list, te) {                                       \
        if (cur->member > (max)->member) {                                              \
            (max) = cur;                                                                \
        }                                                                               \
    }                                                                                   \
}

#define S_LIST_FIND_ID(type, list, id, sensor_inst)   {                                 \
    sensor_inst_t *cur;                                                                 \
    (sensor_inst) = NULL;                                                               \
    TAILQ_FOREACH(cur, &S_TYPE(type)->list, te)  {                                      \
        if (cur->sensor_id == (id)) {                                                   \
            (sensor_inst) = cur;                                                        \
            break;                                                                      \
        }                                                                               \
    }                                                                                   \
}

#define A_LIST                          (&__sensormng.actuator_list)

#define A_MUTEX(stmts)                  if (xSemaphoreTake(A_LIST->mutex, portMAX_DELAY)) { \
                                            stmts;                                      \
                                            xSemaphoreGive(A_LIST->mutex);              \
                                        }

#if CONFIG_CAN5_HIGH_FREQ_IMU_ENABLE
#define HIGH_FREQ_IMU                   (&__sensormng.high_freq)
#endif

ESP_EVENT_DEFINE_BASE(CAN5_EVT_SENSORMNG);

/* ---------------------------------------------------------------------
 * Forward declarations
 -----------------------------------------------------------------------*/
portTASK_FUNCTION(__task_sensors_type, pv);

portTASK_FUNCTION(__task_sensormng, pv);

portTASK_FUNCTION(__task_high_freq_imu, pv);


static can5_err_t __allocate_sensor(allocate_params_t * params);

static can5_err_t __allocate_actuator(bool is_realtime);

/* ---------------------------------------------------------------------
 * Function definition
 -----------------------------------------------------------------------*/


static can5_err_t init()
{
    TRACE_FUNC;

    if (__sensormng.status == SENSORMNG_STAT_INITD) {
        return CAN5_SUCCESS;
    }

    TAILQ_INIT(&__sensormng.scheduled_cycle_data.data_list);
    __sensormng.scheduled_cycle_data.mutex = xSemaphoreCreateMutex();
    VERIFY_NOT_NULL(__sensormng.scheduled_cycle_data.mutex);

    __sensormng.evt_grp = xEventGroupCreate();
    VERIFY_NOT_NULL(__sensormng.evt_grp);

    for (can5_phy_io_type_t type = 0; type < CAN5_PHY_IO_TYPE_COUNT; type++) {
        S_TYPE(type)->type = type;
        TAILQ_INIT(&S_TYPE(type)->undetected);
        TAILQ_INIT(&S_TYPE(type)->ready);
        TAILQ_INIT(&S_TYPE(type)->running);
        S_TYPE(type)->mutex = xSemaphoreCreateMutex();
        VERIFY_NOT_NULL(S_TYPE(type)->mutex);
    }

    TAILQ_INIT(&A_LIST->undetected);
    TAILQ_INIT(&A_LIST->detected);
    A_LIST->mutex = xSemaphoreCreateMutex();

#if CONFIG_IDF_TARGET_ESP32
    // disable latch of all ad ports
    for (can5_port_idx_t port = ADPORT_0; port < ADPORT_COUNT; port++) {
        VERIFY_SUCCESS(hal.enable(false, port));
        taskYIELD();
    }
#endif

    // disable latch of all uart ports
    for (can5_port_idx_t port = UPORT_0; port < UPORT_COUNT; port++) {
        VERIFY_SUCCESS(hal.enable(false, port));
        taskYIELD();
    }

    __sensormng.sensormng_task.hdl = xTaskCreateStatic(__task_sensormng,
                                                       "__task_sensormng",
                                                       CONFIG_CAN5_SENSORMNG_TASK_STACK_SIZE,
                                                       NULL,
                                                       CONFIG_CAN5_SENSORMNG_TASK_PRIORITY,
                                                       task_stacks.sensormng,
                                                       &__sensormng.sensormng_task.buffer);

    VERIFY_NOT_NULL(__sensormng.sensormng_task.hdl);

    S_I2C->task.hdl = xTaskCreateStatic(__task_sensors_type,
                                        "__task_i2c_sensors",
                                        CONFIG_CAN5_SENSORMNG_I2C_TASK_STACK_SIZE,
                                        S_I2C,
                                        CONFIG_CAN5_SENSORMNG_I2C_TASK_PRIORITY,
                                        task_stacks.i2c,
                                        &S_I2C->task.buffer);

    VERIFY_NOT_NULL(S_I2C->task.hdl);

#if CONFIG_CAN5_SENSORMNG_ADC_TASK_ENABLE
    S_ADC->task.hdl = xTaskCreateStatic(__task_sensors_type,
                                        "__task_adc_sensors",
                                        CONFIG_CAN5_SENSORMNG_ADC_TASK_STACK_SIZE,
                                        S_ADC,
                                        CONFIG_CAN5_SENSORMNG_ADC_TASK_PRIORITY,
                                        task_stacks.adc,
                                        &S_ADC->task.buffer);

    VERIFY_NOT_NULL(S_ADC->task.hdl);
#endif

    S_UART->task.hdl = xTaskCreateStatic(__task_sensors_type,
                                         "__task_uart_sensors",
                                         CONFIG_CAN5_SENSORMNG_UART_TASK_STACK_SIZE,
                                         S_UART,
                                         CONFIG_CAN5_SENSORMNG_UART_TASK_PRIORITY,
                                         task_stacks.uart,
                                         &S_UART->task.buffer);

    VERIFY_NOT_NULL(S_UART->task.hdl);
/*
    esp_timer_create_args_t timer_args = {
        .name = "sensor timer",
        .skip_unhandled_events = true,
        .arg = NULL,
    };

    VERIFY_SUCCESS(esp_timer_start_periodic(__sensormng.timer, ))
  */
    __sensormng.status = SENSORMNG_STAT_INITD;


    return CAN5_SUCCESS;
}

static can5_err_t uninit()
{
    TRACE_FUNC;
    return CAN5_SUCCESS;
}


static can5_err_t run()
{
    return CAN5_SUCCESS;

}

static can5_err_t pre_sleep()
{
    return CAN5_SUCCESS;
}

static can5_err_t post_sleep()
{
    return CAN5_SUCCESS;
}

static bool is_sleepable()
{
    return CAN5_SUCCESS;
}

static bool is_sleeping()
{
    return false;
}

static int32_t status_get()
{
    return __sensormng.status;
}

static can5_err_t enable_sensor(bool enable, uint8_t sensor_id)
{
    return CAN5_SUCCESS;
}

static time_t read_time(void)
{
    return 10;
}

static time_t warm_up_time(void)
{
    return 5;
}

static can5_err_t warm_up_time_array(can5_sensormng_wake_up_time_t *wake_up, size_t *len)
{
    *len = 0;
    return CAN5_SUCCESS;
}

static can5_err_t get_ready_sensors(can5_sensor_meta_details_t *sensors_meta, size_t *len)
{
    TRACE_FUNC;

    sensor_inst_t *cur;
    *len = 0;

    for (can5_phy_io_type_t type = 0 ; type < CAN5_PHY_IO_TYPE_COUNT; type++) {

        TAILQ_FOREACH(cur, &S_TYPE(type)->ready, te) {
            sensors_meta[*len].version = cur->sensordriv->details.version;
            sensors_meta[*len].name = cur->sensordriv->details.name;
            sensors_meta[*len].manufacturer = cur->sensordriv->details.manufacturer;
            sensors_meta[*len].last_reading = cur->last_reading;
            sensors_meta[*len].port = cur->port;
            sensors_meta[*len].type = cur->sensordriv->details.sensor.type;

            switch (sensors_meta[*len].type) {
                case CAN5_SENSORDRIV_TYPE_WS3226:
                    sensors_meta[*len].serial_num = malloc(10);     // 10 bytes in WS3226
                    get_ws3226_id(cur->hdl, &sensors_meta[*len].serial_num);
                    break;
                default:
                    sensors_meta[*len].serial_num = NULL;
                    break;

            }
            (*len)++;
        }

        TAILQ_FOREACH(cur, &S_TYPE(type)->running, te) {
            sensors_meta[*len].version = cur->sensordriv->details.version;
            sensors_meta[*len].name = cur->sensordriv->details.name;
            sensors_meta[*len].manufacturer = cur->sensordriv->details.manufacturer;
            sensors_meta[*len].last_reading = cur->last_reading;
            sensors_meta[*len].port = cur->port;
            sensors_meta[*len].type = cur->sensordriv->details.sensor.type;

            switch (sensors_meta[*len].type) {
                case CAN5_SENSORDRIV_TYPE_WS3226:
                    sensors_meta[*len].serial_num = malloc(10);     // 10 bytes in WS3226
                    get_ws3226_id(cur->hdl, &sensors_meta[*len].serial_num);
                    break;
                default:
                    sensors_meta[*len].serial_num = NULL;
                    break;

            }
            (*len)++;
        }
    }

    return CAN5_SUCCESS;
}


static can5_err_t run_sensor_commands(const can5_sensordriv_type_t sensor_type, const int cmd, const void *params, void *response)
{

    sensor_inst_t *cur;

    for (can5_phy_io_type_t type = 0 ; type < CAN5_PHY_IO_TYPE_COUNT; type++)
    {
        TAILQ_FOREACH(cur, &S_TYPE(type)->ready, te) {
            if (cur->sensordriv->details.sensor.type == sensor_type && cur->sensordriv->ops.is_running(cur->hdl)) {
                return cur->sensordriv->ops.driverctl(cur->hdl, cmd, params, response);
            }
        }
    }

    return CAN5_SENSOR_ERR_INVALID_PORT;
}
/* ---------------------------------------------------------------------
 * Event Handlers
 -----------------------------------------------------------------------*/
/* ---------------------------------------------------------------------
 * Private Functions
 -----------------------------------------------------------------------*/
can5_err_t dispatch_command(can5_actuator_cmd_t cmd, can5_actuator_cmd_params_t *params)
{
    actuator_inst_t *actuator_inst;
    for (size_t i = 0; i < sizeof(__actuator_map)/ sizeof(__actuator_map[0]); i ++) {
        if (__actuator_map[i].cmd == cmd) {
            TAILQ_FOREACH(actuator_inst, &A_LIST->detected, te) {
                if (actuator_inst->port == params->port &&
                    __actuator_map[i].type == actuator_inst->actuatordriv->details.actuator.type) {
                    return actuator_inst->actuatordriv->ops.command(actuator_inst->hdl, cmd, params);
                }
            }
        }
    }

    return CAN5_ERR_INVALID_PARAM;
}

static int64_t __get_cycle_period(void) {
    static int64_t cycle_time = 0;

    if (cycle_time != 0) {
        return cycle_time;
    }

    if (config_manager.read_int(CFG_DATA_CYCLE_SEC, &cycle_time) != CAN5_SUCCESS) {
        ESP_LOGE(TAG, "Cannot retrive cycle time.");
        cycle_time = 120;
    }
    return cycle_time;
}

static void __sleep_until(time_t sleep_ms)
{
    TRACE_FUNC;
    esp_event_post(CAN5_EVT_SENSORMNG, CAN5_SENSORMNG_EVT_SLEEP_UNTIL, &sleep_ms, sizeof(time_t), 5);
}

static io_port_t *__get_io_ports(size_t *len)
{
    io_port_t *io_ports;
    int i = 0;
    bool enable_sim_gps;
    *len = CFG_UART_7 - CFG_ADC_0 + 1 ;

    io_ports = NULL;
    int sim7600_pos = -1;

    VERIFY_ALLOC_SAFENORETURN(
            io_ports,
            sizeof(io_port_t) * *len,
            {
                io_ports = NULL;
                goto done;
            });


    for (can5_cfg_type_t type = CFG_ADC_0; type <= CFG_UART_7; type++) {
        char *val;
        can5_port_idx_t port;

        val = NULL;
        if (config_read(type, (uint8_t **) &val, NULL) != CAN5_SUCCESS) {
            FREE_BULK(io_ports, val);
            io_ports = NULL;
            return io_ports;
        }

        for (can5_sensordriv_type_t s = 0; s < CAN5_SENSORDRIV_TYPE_COUNT; s++) {
            const char *sensor_str = can5_sensor_type_getstr(s);
            if (!strcmp(val, sensor_str)) {
                io_ports[i].sensor = s;
                io_ports[i].is_sensor = true;
                break;
            }
        }

        if (!io_ports[i].is_sensor) {
            for (can5_actuatordriv_type_t a = 0; a < CAN5_ACTUATORDRIV_TYPE_COUNT; a++) {
                const char *actuator_str = can5_actuator_type_getstr(a);
                if (!strcmp(val, actuator_str)) {
                    io_ports[i].actuator = a;
                    io_ports[i].is_actuator = true;
                    break;
                }
            }
        }

        /*
        if (!(config.io_ports[i].is_sensor | config.io_ports[i].is_actuator)) {
            config.io_ports[i].sensor = CAN5_SENSORDRIV_TYPE_NONE;
        }
        */

        free(val);

        port = CAN5_PORT_NULL;

        if (CFG_ADC_0 <= type && type <= CFG_ADC_3) {
            port = ADPORT_0 + (type - CFG_ADC_0);
        }
        else if (CFG_UART_0 <= type && type <= CFG_UART_7) {
            port = UPORT_0 + (type - CFG_UART_0);
        }
        else if (CFG_I2C_0 <= type && type <= CFG_I2C_7) {
            if (sim7600_pos == -1 && !io_ports[i].is_sensor && !io_ports[i].is_actuator) {
                sim7600_pos = i;
            }
            port = I2C_0;
        }

        io_ports[i].port = port;
        i++;
    }

    if (config_read_bool(CFG_CELL_ENABLE_GPS, &enable_sim_gps) == CAN5_SUCCESS)
    {
        if (enable_sim_gps && sim7600_pos != -1)
        {
            io_ports[sim7600_pos].sensor = CAN5_SENSORDRIV_TYPE_SIM7600_GPS;
            io_ports[sim7600_pos].is_sensor = true;
            io_ports[sim7600_pos].port = I2C_0;
        }
    }


done:
    return io_ports;
}

static void __print_sensors_list(sensor_inst_list_head_t *head)
{
    TRACE_FUNC;
    ESP_LOGI(TAG, "head %p", head);
    sensor_inst_t *n;
    TAILQ_FOREACH(n, head, te) {
        ESP_LOGI(TAG, "%s port %s", can5_sensor_type_getstr(n->sensordriv->details.sensor.type),
                 can5_hal_port_getstr(n->port));
    }
}

static void __print_actuators_list(actuator_inst_list_head_t *head)
{
    TRACE_FUNC;
    ESP_LOGI(TAG, "head %p", head);
    actuator_inst_t *n;

    TAILQ_FOREACH(n, head, te) {
        ESP_LOGI(TAG, "%s port %s", can5_actuator_type_getstr(n->actuatordriv->details.actuator.type),
                 can5_hal_port_getstr(n->port));
    }
}

static can5_err_t __allocate_sensor(allocate_params_t *params)
{
    TRACE_FUNC;

    io_port_t *io_ports;
    size_t ports_len;

    io_ports = __get_io_ports(&ports_len);

    uint8_t id_counter;

    if (!io_ports) {
        return CAN5_CFG_ERR_GET;
    }

    id_counter = 0;
    /* TODO: Handle duplicate drivers */
    for (size_t i = 0; i < ports_len; i++) {
        io_port_t *io_port = &io_ports[i];


        if (io_port->is_sensor) {
            bool found;
            sensor_inst_t *sensor_inst;

            ESP_LOGI(TAG, "Port %s <--> Sensor Type %s",
                     can5_hal_port_getstr(io_port->port),
                     can5_sensor_type_getstr(io_port->sensor));

            VERIFY_ALLOC(sensor_inst, sizeof(sensor_inst_t));

            sensor_inst->sensordriv = NULL;
            sensor_inst->port = io_port->port;
            sensor_inst->is_realtime = params->is_realtime;

            found = false;
            for (int k = 0; __sensormng.sensor_driv[k] != NULL; k++) {

                if (__sensormng.sensor_driv[k]->details.sensor.type == io_port->sensor) {
                    size_t sensor_hdl_len;
                    /* The configured driver for the sensor, now try to detect */
                    can5_phy_io_type_t io_type = __sensormng.sensor_driv[k]->details.io_type;
                    sensor_inst->sensordriv = __sensormng.sensor_driv[k];
                    sensor_inst->sensor_id = id_counter++;
                    sensor_inst->hdl = sensor_inst->sensordriv->ops.alloc(&sensor_hdl_len);

                    /* set the sensor id */
                    sensor_inst->sensordriv->ops.set_id(sensor_inst->hdl, sensor_inst->sensor_id);

#if CONFIG_CAN5_HIGH_FREQ_IMU_ENABLE
                    if (params->is_high_freq_imu && sensor_inst->sensordriv->details.sensor.type == CAN5_SENSORDRIV_TYPE_MPU6500) {
                        params->out_imu_sensor_inst = sensor_inst;
                        ESP_LOGI(TAG, "Adding %s to high_freq_imu", can5_sensor_type_getstr(sensor_inst->sensordriv->details.sensor.type));

                    }
                    else {
                        S_MUTEX_TYPE(io_type,
                                     TAILQ_INSERT_TAIL(&S_TYPE(io_type)->undetected, sensor_inst, te));
                        ESP_LOGI(TAG, "Adding %s to %d", can5_sensor_type_getstr(sensor_inst->sensordriv->details.sensor.type),
                                 io_type);

                    }
#else
                    S_MUTEX_TYPE(io_type,
                                     TAILQ_INSERT_TAIL(&S_TYPE(io_type)->undetected, sensor_inst, te));
                        ESP_LOGI(TAG, "Adding %s to %d", can5_sensor_type_getstr(sensor_inst->sensordriv->details.sensor.type),
                                 io_type);
#endif

                    found = true;
                    break;
                }
            }

            if (!found) {
                free(sensor_inst);
            }
        }
    }

#if CONFIG_CAN5_SENSORMNG_ADC_TASK_ENABLE
    __print_sensors_list(&S_ADC->undetected);
#endif
    __print_sensors_list(&S_UART->undetected);
    __print_sensors_list(&S_I2C->undetected);

    free(io_ports);

    return CAN5_SUCCESS;
}

static can5_err_t __allocate_actuator(bool is_realtime)
{
    TRACE_FUNC;

    io_port_t *io_ports;
    size_t ports_len;

    io_ports = __get_io_ports(&ports_len);

    uint8_t id_counter;

    if (!io_ports) {
        return CAN5_CFG_ERR_GET;
    }

    id_counter = 0;

    for (size_t i = 0; i < ports_len; i++) {
        io_port_t *io_port = &io_ports[i];

        if (io_port->is_actuator) {
            bool found;
            actuator_inst_t *actuator_inst;

            ESP_LOGI(TAG, "Port %s <--> Sensor Type %s",
                     can5_hal_port_getstr(io_port->port),
                     can5_actuator_type_getstr(io_port->actuator));

            VERIFY_ALLOC(actuator_inst, sizeof(actuator_inst_t));

            actuator_inst->actuatordriv = NULL;
            actuator_inst->port = io_port->port;
            //actuator_inst->is_realtime = is_realtime;

            found = false;
            for (int k = 0; __sensormng.actuator_driv[k] != NULL; k++) {
                if (__sensormng.actuator_driv[k]->details.actuator.type == io_port->actuator) {
                    size_t actuator_hdl_len;

                    can5_phy_io_type_t io_type = __sensormng.actuator_driv[k]->details.io_type;
                    actuator_inst->actuatordriv = __sensormng.actuator_driv[k];
                    actuator_inst->id = id_counter++;
                    actuator_inst->hdl = actuator_inst->actuatordriv->ops.alloc(&actuator_hdl_len);

                    actuator_inst->actuatordriv->ops.set_id(actuator_inst->hdl, actuator_inst->id);

                    TAILQ_INSERT_TAIL(&A_LIST->undetected, actuator_inst, te);

                    ESP_LOGI(TAG, "Adding %s to %d", can5_actuator_type_getstr(actuator_inst->actuatordriv->details.actuator.type),
                             io_type);
                    found = true;
                    break;
                }
            }

            if (!found) {
                free(actuator_inst);
            }

        }
    }

    __print_actuators_list(&A_LIST->undetected);

    free(io_ports);

    return CAN5_SUCCESS;
}

static void __hw_evt_cb(const can5_sensor_hw_evt_t *evt)
{
    TRACE_FUNC;

    sensor_inst_t *sensor_inst;
    can5_phy_io_type_t type;
    sensor_inst_list_head_t *curr_head = NULL;
    for (type = 0; type < CAN5_PHY_IO_TYPE_COUNT; type++) {
        // search running list
        curr_head = &S_TYPE(type)->running;
        S_LIST_FIND_ID(type, running, evt->sensor_id, sensor_inst);
        if (sensor_inst) {
            break;
        }
        // search ready list
        curr_head = &S_TYPE(type)->ready;
        S_LIST_FIND_ID(type, ready, evt->sensor_id, sensor_inst);
        if (sensor_inst) {
            break;
        }
    }

    if (!sensor_inst) {
        ESP_LOGE(TAG, "Cannot find sensor %s",
                 can5_sensor_type_getstr(sensor_inst->sensordriv->details.sensor.type));
    }

    switch (evt->type) {

        case CAN5_SENSOR_HWEVT_NONE:
            break;

        case CAN5_SENSOR_HWEVT_UNIMPLIMENTED:
            ESP_LOGE(TAG, "Sensor %s got HW Event %s",
                     can5_sensor_type_getstr(sensor_inst->sensordriv->details.sensor.type),
                     sensor_hwevt_type_getstr(evt->type));
            if (!sensor_inst->is_realtime) {
                ESP_LOGI(TAG, "hw evt cb DISABLE sensor %s on port %s",
                         can5_sensor_type_getstr(sensor_inst->sensordriv->details.sensor.type),
                         can5_hal_port_getstr(sensor_inst->port));
                sensor_inst->sensordriv->ops.enable(sensor_inst->hdl, false);
            }

            S_MUTEX_TYPE(type, {
                TAILQ_REMOVE(curr_head, sensor_inst, te);
                TAILQ_INSERT_TAIL(&S_TYPE(type)->ready, sensor_inst, te);
            });
            break;

        case CAN5_SENSOR_HWEVT_READING:
            break;

        case CAN5_SENSOR_HWEVT_READ_FAILURE:
            ESP_LOGE(TAG, "Sensor %s got HW Event %s",
                     can5_sensor_type_getstr(sensor_inst->sensordriv->details.sensor.type),
                     sensor_hwevt_type_getstr(evt->type));

            if (!sensor_inst->is_realtime) {
                ESP_LOGI(TAG, "hw evt cb DISABLE sensor %s on port %s",
                         can5_sensor_type_getstr(sensor_inst->sensordriv->details.sensor.type),
                         can5_hal_port_getstr(sensor_inst->port));
                sensor_inst->sensordriv->ops.enable(sensor_inst->hdl, false);
            }

            S_MUTEX_TYPE(type, {
                TAILQ_REMOVE(curr_head, sensor_inst, te);
                TAILQ_INSERT_TAIL(&S_TYPE(type)->undetected, sensor_inst, te);
            });
            break;

        case CAN5_SENSOR_HWEVT_READ_COMPLETE:
            ESP_LOGI(TAG, "Sensor %s got HW Event %s",
                     can5_sensor_type_getstr(sensor_inst->sensordriv->details.sensor.type),
                     sensor_hwevt_type_getstr(evt->type));

            if (!sensor_inst->is_realtime) {
                ESP_LOGI(TAG, "hwt evt cb DISABLE sensor %s on port %s",
                         can5_sensor_type_getstr(sensor_inst->sensordriv->details.sensor.type),
                         can5_hal_port_getstr(sensor_inst->port));
                sensor_inst->sensordriv->ops.enable(sensor_inst->hdl, false);
            }

            S_MUTEX_TYPE(type, {
                TAILQ_REMOVE(curr_head, sensor_inst, te);
                TAILQ_INSERT_TAIL(&S_TYPE(type)->ready, sensor_inst, te);
            });
            break;

        default:
            break;
    }
}

static can5_err_t __save_data_to_storage()
{
    TRACE_FUNC;

    can5_err_t ret;
    static char sensor_cycle_str[CAN5_STORAGE_MAX_LEN]; // let it be static as this is used only for the duration of
                                                        // this function
    size_t sensor_cycle_str_len;

    sensor_cycle_str_len = 0;

    S_MUTEX(__sensormng.scheduled_cycle_data.mutex,
            sensor_cycle_str_len = can5_sensor_data_list_dumps(&__sensormng.scheduled_cycle_data.data_list, sensor_cycle_str));

    ret = CAN5_SUCCESS;

    if (sensor_cycle_str_len > 0) {
        ESP_LOGI(TAG, "Pushing: %s", sensor_cycle_str);
        ret = can5_storage_push_fs(SENSOR_DATA_TAG,
                                               (uint8_t *) sensor_cycle_str,
                                               sensor_cycle_str_len);
        CAN5_ERR_CHECK_NO_ABORT(ret);
    }

    if (ret == CAN5_SUCCESS) {

        esp_event_post(CAN5_EVT_SENSORMNG,
                       CAN5_SENSORMNG_EVT_CYCLE_SAVED,
                       NULL,
                       0,
                       5);
    }


    S_MUTEX(__sensormng.scheduled_cycle_data.mutex,
        can5_sensor_data_list_free(&__sensormng.scheduled_cycle_data.data_list));

    //return config_manager.update_counter("data", true);
    return CAN5_SUCCESS;
}


/* ADD sensor parsers here, to make sense of sensor data, used for last reading */
static void __read_cb(const uint8_t sensor_id, const void *data, const size_t len)
{
    TRACE_FUNC;

    char sensor_data[CAN5_SENSOR_PARSED_DATA_MAX_LEN];
    size_t sensor_data_len;
    sensor_inst_t *sensor_inst;
    can5_phy_io_type_t type;
    can5_sensor_data_list_t *list;
    const can5_sensor_ublox_neo_data_t *gps_data;
    const can5_sensor_sim7600_gps_data_t *cell_gps_data;

    for (type = 0; type < CAN5_PHY_IO_TYPE_COUNT; type++) {
        // search running list
        S_LIST_FIND_ID(type, running, sensor_id, sensor_inst);
        if (sensor_inst) {
            break;
        }
        // search ready list, because hw event may have moved it here depending on which hook sensor
        // driver calls first
        S_LIST_FIND_ID(type, ready, sensor_id, sensor_inst);
        if (sensor_inst) {
            break;
        }
    }

    if (sensor_inst == NULL) {
        ESP_LOGE(TAG, "Invalid read callback for sensor id %d", sensor_id);
        return;
    }


    assert(data);
    CLEAR_ARRAY(sensor_data);

    list = NULL;
    list = sensor_inst->sensordriv->ops.get_sensor_data(sensor_inst->hdl, data, len);

    if (list) {
        S_MUTEX(__sensormng.scheduled_cycle_data.mutex,
                TAILQ_CONCAT(&__sensormng.scheduled_cycle_data.data_list, list, te));
        free(list);
    }

    switch (sensor_inst->sensordriv->details.sensor.type) {

        case CAN5_SENSORDRIV_TYPE_PM:

            parse_pm_read_data(sensor_inst->sensordriv, data, sensor_data, &sensor_data_len);
            break;

        case CAN5_SENSORDRIV_TYPE_CO2:

            parse_co2_read_data(sensor_inst->sensordriv, data, sensor_data, &sensor_data_len);
            break;

        case CAN5_SENSORDRIV_TYPE_GPS:

            gps_data = data;

            if (gps_data->fix) {

                parse_gps_read_data(sensor_inst->sensordriv, data, sensor_data, &sensor_data_len);

                __sensormng.scheduled_cycle_data.gps_set = true;

                // update gps data
                CAN5_ERR_CHECK_NO_ABORT(config_manager.write_double(CFG_LAST_G_LAT, gps_data->lat));
                CAN5_ERR_CHECK_NO_ABORT(config_manager.write_double(CFG_LAST_G_LNG, gps_data->lng));
                CAN5_ERR_CHECK_NO_ABORT(config_manager.write_double(CFG_LAST_G_ALT, gps_data->alt));
                time_t now = time(NULL);
                CAN5_ERR_CHECK_NO_ABORT(config_manager.write_int(CFG_LAST_G_TIME, now));

            }

            break;

        case CAN5_SENSORDRIV_TYPE_CO_ADC:

            parse_co_adc_read_data(sensor_inst->sensordriv, data, sensor_data, &sensor_data_len);
            break;

        case CAN5_SENSORDRIV_TYPE_CO:

            parse_co_read_data(sensor_inst->sensordriv, data, sensor_data, &sensor_data_len);
            break;

        case CAN5_SENSORDRIV_TYPE_BME680:

            break;

        case CAN5_SENSORDRIV_TYPE_BME280:

            parse_bme280_read_data(sensor_inst->sensordriv, data, sensor_data, &sensor_data_len);
            break;

        case CAN5_SENSORDRIV_TYPE_NO2:

            parse_no2_read_data(sensor_inst->sensordriv, data, sensor_data, &sensor_data_len);
            break;

        case CAN5_SENSORDRIV_TYPE_SIM7600_GPS:

            cell_gps_data = data;
            if (cell_gps_data->fix) {

                parse_sim7600_gps_read_data(sensor_inst->sensordriv, data, sensor_data, &sensor_data_len);

                __sensormng.scheduled_cycle_data.gps_set = true;

                // update gps data
                CAN5_ERR_CHECK_NO_ABORT(config_manager.write_double(CFG_LAST_G_LAT, cell_gps_data->lat));
                CAN5_ERR_CHECK_NO_ABORT(config_manager.write_double(CFG_LAST_G_LNG, cell_gps_data->lng));
                CAN5_ERR_CHECK_NO_ABORT(config_manager.write_double(CFG_LAST_G_ALT, cell_gps_data->alt));
                time_t now = time(NULL);
                CAN5_ERR_CHECK_NO_ABORT(config_manager.write_int(CFG_LAST_G_TIME, now));
            }

            break;

        case CAN5_SENSORDRIV_TYPE_WS3226:

            parse_ws3226_read_data(sensor_inst->sensordriv, data, sensor_data, &sensor_data_len);
            break;

        case CAN5_SENSORDRIV_TYPE_MPU6500:

            parse_mpu6500_read_data(sensor_inst->sensordriv, data, sensor_data, &sensor_data_len);
            break;

        case CAN5_SENSORDRIV_TYPE_SCD41:

            parse_scd41_read_data(sensor_inst->sensordriv, data, sensor_data, &sensor_data_len);
            break;

        default:
            break;
    }

    // choose between realtime or scheduled

    strcpy(sensor_inst->last_reading, sensor_data);
}

/* ---------------------------------------------------------------------
 * Tasks
 -----------------------------------------------------------------------*/

/*
 * The following enum is multiplied by CAN5_SENSOR_IO_TYPE_COUNT
 * hence the condition (TASK_NOTIF_COUNT * CAN5_SENSOR_IO_TYPE_COUNT) <  bits(EventBits_t) should hold
 * for esp32, it is number of bits in EventBits_t is 32.
 */
typedef enum task_notif_e {
    TASK_NOTIF_DETECT = 0,
    TASK_NOTIF_READ,
    TASK_NOTIF_RUN,
    TASK_NOTIF_SUSPEND,
    TASK_NOTIF_DETECT_DONE,
    TASK_NOTIF_READ_DONE,
    TASK_NOTIF_RUN_DONE,
    TASK_NOTIF_SUSPEND_DONE,
} task_notif_t;

#define NOTIF_TO_TYPE(type, notif)      (((notif) * CAN5_PHY_IO_TYPE_COUNT) + (type))
#define NOTIF_TO_TYPE_BIT(type, notif)  BIT(NOTIF_TO_TYPE(type, notif))


#if CONFIG_CAN5_SENSORMNG_ADC_TASK_ENABLE
#define NOTIF_TO_ALL_BIT(notif)         (NOTIF_TO_TYPE_BIT(CAN5_PHY_IO_TYPE_ADC, notif)  |   \
                                         NOTIF_TO_TYPE_BIT(CAN5_PHY_IO_TYPE_I2C, notif)  |   \
                                         NOTIF_TO_TYPE_BIT(CAN5_PHY_IO_TYPE_UART, notif))
#else
#define NOTIF_TO_ALL_BIT(notif)         (NOTIF_TO_TYPE_BIT(CAN5_PHY_IO_TYPE_I2C, notif)  |   \
                                         NOTIF_TO_TYPE_BIT(CAN5_PHY_IO_TYPE_UART, notif))
#endif

#define NOTIF_BITS_ALL                  (NOTIF_TO_ALL_BIT(TASK_NOTIF_DETECT)            | \
                                         NOTIF_TO_ALL_BIT(TASK_NOTIF_READ)              | \
                                         NOTIF_TO_ALL_BIT(TASK_NOTIF_RUN)               | \
                                         NOTIF_TO_ALL_BIT(TASK_NOTIF_SUSPEND)           | \
                                         NOTIF_TO_ALL_BIT(TASK_NOTIF_DETECT_DONE)       | \
                                         NOTIF_TO_ALL_BIT(TASK_NOTIF_READ_DONE)         | \
                                         NOTIF_TO_ALL_BIT(TASK_NOTIF_RUN_DONE)          | \
                                         NOTIF_TO_ALL_BIT(TASK_NOTIF_SUSPEND_DONE))

static void __detect_and_initialize(sensor_inst_list_t *list, int retry, bool honor_interval)
{
    sensor_inst_t *sensor_inst, *next;
    time_t now;

    while (retry-- != 0 && !TAILQ_EMPTY(&list->undetected)) {

        TAILQ_FOREACH_SAFE(sensor_inst, &list->undetected, te, next) {
            now = can5_time(NULL);
            if (honor_interval && sensor_inst->last_detect_time + DETECT_INTERVAL > now) {
                continue;
            }

            sensor_inst->last_detect_time = now;

            if (!sensor_inst->is_realtime) {
                ESP_LOGI(TAG, "detect ENABLE sensor %s on port %s",
                         can5_sensor_type_getstr(sensor_inst->sensordriv->details.sensor.type),
                         can5_hal_port_getstr(sensor_inst->port));
                hal.enable(true, sensor_inst->port);
            }

            if (sensor_inst->sensordriv->ops.detect(sensor_inst->hdl, sensor_inst->port) == CAN5_SUCCESS) {

                if (sensor_inst->sensordriv->ops.init(sensor_inst->hdl, sensor_inst->port, __hw_evt_cb) == CAN5_SUCCESS) {
                    sensor_inst->sensordriv->ops.register_read_cb(sensor_inst->hdl, __read_cb);
                    S_MUTEX(list->mutex, {
                        TAILQ_REMOVE(&list->undetected, sensor_inst, te);
                        TAILQ_INSERT_TAIL(&list->ready, sensor_inst, te);
                    });
                } else {

                    ESP_LOGE(TAG, "Unable to initialize %s on port %s",
                             can5_sensor_type_getstr(sensor_inst->sensordriv->details.sensor.type),
                             can5_hal_port_getstr(sensor_inst->port));
                }
            } else {

                ESP_LOGE(TAG, "Unable to detect %s on port %s",
                         can5_sensor_type_getstr(sensor_inst->sensordriv->details.sensor.type),
                         can5_hal_port_getstr(sensor_inst->port));
            }

            // do not disable in detect
            if (!sensor_inst->is_realtime && false) {
                hal.enable(false, sensor_inst->port);
                ESP_LOGI(TAG, "Detect DISABLE sensor %s on port %s",
                         can5_sensor_type_getstr(sensor_inst->sensordriv->details.sensor.type),
                         can5_hal_port_getstr(sensor_inst->port));
            }

            taskYIELD();
        }
    }
}

static void __scheduled_reset_cycle_data()
{
    time_t now;
    can5_sensor_data_t *timestamp;

    // get system UTC time

    if (!TAILQ_EMPTY(&__sensormng.scheduled_cycle_data.data_list)) {
        ESP_LOGE(TAG, "Cycle data is not empty at the start of the cycle.");
        S_MUTEX(__sensormng.scheduled_cycle_data.mutex,
                can5_sensor_data_list_free(&__sensormng.scheduled_cycle_data.data_list));
    }

    __sensormng.scheduled_cycle_data.gps_set = false;

    now = time(NULL);

    __sensormng.scheduled_cycle_data.timestamp = now;

    timestamp = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_TIMESTAMP, CAN5_PORT_NULL,
                                         CAN5_SENSOR_DATA_DATATYPE_NUM, NULL, now, 0);
    if (timestamp) {
        S_MUTEX(__sensormng.scheduled_cycle_data.mutex,
                TAILQ_INSERT_HEAD(&__sensormng.scheduled_cycle_data.data_list, timestamp, te));
    }

}

static void __read_sensor_data(sensor_inst_list_t *list)
{
    sensor_inst_t *sensor_inst, *next;

    TAILQ_FOREACH_SAFE(sensor_inst, &list->ready, te, next) {
        time_t read_time_end_ms = can5_sec_to_ms(sensor_inst->sensordriv->details.sensor.read_time);
        if (sensor_inst->sensordriv->details.sensor.read_time == CAN5_SENSOR_TIME_MAX) {
            read_time_end_ms = can5_sec_to_ms(__get_cycle_period());
        }

        if (sensor_inst->is_realtime) {
            read_time_end_ms = 0;
        }

        if (sensor_inst->sensordriv->details.sensor.type == CAN5_SENSORDRIV_TYPE_WS3226) {
            // reduce one minute from the read time
            if (read_time_end_ms % WS3226_POLL_INTERVAL == 0) {
                read_time_end_ms -= WS3226_POLL_INTERVAL;
                read_time_end_ms = read_time_end_ms < 0 ? 0: read_time_end_ms;
            }
            else {
                read_time_end_ms -= (read_time_end_ms % WS3226_POLL_INTERVAL);
            }

            // one second more to process pending stuffs
            read_time_end_ms += 1000;
        }


        if (sensor_inst->sensordriv->ops.read(sensor_inst->hdl, NULL, NULL, can5_time_ms(NULL) + read_time_end_ms, false) == CAN5_SUCCESS) {
            // move sensor to running queue
            S_MUTEX(list->mutex, {
                TAILQ_REMOVE(&list->ready, sensor_inst, te);
                TAILQ_INSERT_TAIL(&list->running, sensor_inst, te);
            })

        } else {
            // move sensor to undetected queue
            ESP_LOGE(TAG, "Cannot read %s", can5_sensor_type_getstr(sensor_inst->sensordriv->details.sensor.type));
            S_MUTEX(list->mutex, {
                TAILQ_REMOVE(&list->ready, sensor_inst, te);
                TAILQ_INSERT_TAIL(&list->undetected, sensor_inst, te);
            })
        }
        taskYIELD();
    }

}

static bool __run_sensor_data(sensor_inst_list_t *list)
{
    sensor_inst_t *sensor_inst, *next;
    if (TAILQ_EMPTY(&list->running)) {
        return false;
    }

    TAILQ_FOREACH_SAFE(sensor_inst, &list->running, te, next) {
        sensor_inst->sensordriv->ops.run(sensor_inst->hdl);
        taskYIELD();
    }
    return true;
}

portTASK_FUNCTION(__task_sensors_type, pv)
{
    sensor_inst_list_t *list = pv;

    for (;;) {
        EventBits_t bits = xEventGroupWaitBits(__sensormng.evt_grp,
                                               NOTIF_TO_TYPE_BIT(list->type, TASK_NOTIF_READ) |
                                               NOTIF_TO_TYPE_BIT(list->type, TASK_NOTIF_RUN) |
                                               NOTIF_TO_TYPE_BIT(list->type, TASK_NOTIF_DETECT) |
                                               NOTIF_TO_TYPE_BIT(list->type, TASK_NOTIF_SUSPEND),
                                               pdTRUE,
                                               pdFALSE,
                                               portMAX_DELAY);

        if (bits & NOTIF_TO_TYPE_BIT(list->type, TASK_NOTIF_READ)) {
            __read_sensor_data(list);

            ESP_LOGI(TAG, "READING..............");
            xEventGroupSetBits(__sensormng.evt_grp,
                               NOTIF_TO_TYPE_BIT(list->type, TASK_NOTIF_READ_DONE));

        } else if (bits & NOTIF_TO_TYPE_BIT(list->type, TASK_NOTIF_RUN)) {

            while (__run_sensor_data(list)) {
                // run detect lazily
                __detect_and_initialize(list, 1, true);
                if (list->type == CAN5_PHY_IO_TYPE_I2C) {
                    // add delay here because i2c is too fast
                    vTaskDelay(pdMS_TO_TICKS(500));
                }
                taskYIELD();
            }

            ESP_LOGI_V(TAG, "Run over..............");
            xEventGroupSetBits(__sensormng.evt_grp,
                               NOTIF_TO_TYPE_BIT(list->type, TASK_NOTIF_RUN_DONE));

        } else if (bits & NOTIF_TO_TYPE_BIT(list->type, TASK_NOTIF_DETECT)) {
            // run detect 2 times
            __detect_and_initialize(list, 5, false);

            xEventGroupSetBits(__sensormng.evt_grp,
                               NOTIF_TO_TYPE_BIT(list->type, TASK_NOTIF_DETECT_DONE));
        } else if (bits & NOTIF_TO_TYPE_BIT(list->type, TASK_NOTIF_SUSPEND)) {

            xEventGroupSetBits(__sensormng.evt_grp,
                               NOTIF_TO_TYPE_BIT(list->type, TASK_NOTIF_SUSPEND_DONE));
        }

        PRINT_TASK_HIGHWATER_MARK(NULL);
    }
}

// use this static memory for building enable device list.
typedef struct enable_dev_s {
    sensor_inst_t *sensor_inst;
    time_t read_warm_up;
    TAILQ_ENTRY(enable_dev_s) te;
} enable_dev_t;

typedef TAILQ_HEAD(enable_dev_head_s, enable_dev_s) enable_dev_head_t;

static enable_dev_t __enable_dev[CAN5_IO_PORT_MAX];

static void __scheduled_enable_calculate(enable_dev_head_t *list)
{
    enable_dev_t *enable_dev;
    time_t max_read_warm_up;
    sensor_inst_t *sensor_inst;
    size_t count;

    TAILQ_INIT(list);
    count = 0;
    CLEAR_ARRAY(__enable_dev);

    ESP_LOGI(TAG, "Enable Phase!");
    // get all active sensors, and sort in descending order of warm up time.
    for (can5_phy_io_type_t type = 0; type < CAN5_PHY_IO_TYPE_COUNT; type++) {

        // iterate over ready sensors
        TAILQ_FOREACH(sensor_inst, &S_TYPE(type)->ready, te) {

            enable_dev_t *dev = &__enable_dev[count];

            dev->sensor_inst = sensor_inst;
            dev->read_warm_up = sensor_inst->sensordriv->details.sensor.read_warm_up;
            // if list is empty add the first item
            if (TAILQ_EMPTY(list)) {
                TAILQ_INSERT_HEAD(list, dev, te);
                count++;
            } else {
                bool found = false;
                // if list has element, do insertion sort in descending order
                TAILQ_FOREACH(enable_dev, list, te) {

                    if (dev->read_warm_up > enable_dev->read_warm_up) {

                        TAILQ_INSERT_BEFORE(enable_dev, dev, te);
                        count++;
                        found = true;
                        break;
                    }
                }

                // if the position is not found, add at the end of list
                if (!found) {
                    TAILQ_INSERT_TAIL(list, dev, te);
                    count++;
                }
            }
        }
    }

    // if none of the sensors need enable
    if (TAILQ_EMPTY(list)) {
        return;
    }

    max_read_warm_up = TAILQ_FIRST(list)->read_warm_up;

    // readjust to ascending order, with correlated time intervals
    TAILQ_FOREACH(enable_dev, list, te) {
        enable_dev->read_warm_up = -(enable_dev->read_warm_up - max_read_warm_up);
    }
}

static void __scheduled_enable()
{
    enable_dev_head_t list;
    enable_dev_t *dev;
    time_t t;

    __scheduled_enable_calculate(&list);

    t = 0;

    while (!TAILQ_EMPTY(&list)) {
        dev = TAILQ_FIRST(&list);
        if (dev->read_warm_up == t) {
            if (!dev->sensor_inst->is_realtime) {
                ESP_LOGI(TAG, "scheduled enable ENABLE sensor %s on port %s",
                         can5_sensor_type_getstr(dev->sensor_inst->sensordriv->details.sensor.type),
                         can5_hal_port_getstr(dev->sensor_inst->port));
                dev->sensor_inst->sensordriv->ops.enable(dev->sensor_inst->hdl, true);
            }
            TAILQ_REMOVE(&list, dev, te);
        } else {
            time_t sleep = (dev->read_warm_up - t) * 1000; // in ms
            ESP_LOGI(TAG, "sleep for %ld", sleep);
            __sleep_until(sleep);
            vTaskDelay(pdMS_TO_TICKS(sleep));
            t = dev->read_warm_up;
        }
        taskYIELD();
    }
}

static void __scheduled_read_phase(int64_t timeout_ms)
{
    EventBits_t bits;

    xEventGroupSetBits(__sensormng.evt_grp, NOTIF_TO_ALL_BIT(TASK_NOTIF_READ));

    bits = xEventGroupWaitBits(__sensormng.evt_grp,
                               NOTIF_TO_ALL_BIT(TASK_NOTIF_READ_DONE),
                               pdTRUE,
                               pdTRUE,
                               pdMS_TO_TICKS(timeout_ms));

    if ((bits & NOTIF_TO_ALL_BIT(TASK_NOTIF_READ_DONE)) != bits) {
        ESP_LOGE(TAG, "error in event bits TASK_NOTIF_READ_DONE %x %lx", bits,
                 NOTIF_TO_ALL_BIT(TASK_NOTIF_READ_DONE));
    }

    PRINT_TASK_HIGHWATER_MARK(NULL);
}

static void __scheduled_run_phase(time_t timeout_ms)
{
    EventBits_t bits;

    ESP_LOGI(TAG, "RUN Phase!");
    xEventGroupSetBits(__sensormng.evt_grp, NOTIF_TO_ALL_BIT(TASK_NOTIF_RUN));

    bits = xEventGroupWaitBits(__sensormng.evt_grp,
                               NOTIF_TO_ALL_BIT(TASK_NOTIF_RUN_DONE),
                               pdTRUE,
                               pdTRUE,
                               pdMS_TO_TICKS(timeout_ms));

    if ((bits & NOTIF_TO_ALL_BIT(TASK_NOTIF_RUN_DONE)) != bits) {
        ESP_LOGE(TAG, "error in event bits TASK_NOTIF_READ_DONE");
    }

    PRINT_TASK_HIGHWATER_MARK(NULL);
}

static void __scheduled_detect_phase(TickType_t timeout)
{
    EventBits_t bits;

    ESP_LOGI(TAG, "DETECT Phase!");
    xEventGroupSetBits(__sensormng.evt_grp, NOTIF_TO_ALL_BIT(TASK_NOTIF_DETECT));

    bits = xEventGroupWaitBits(__sensormng.evt_grp,
                               NOTIF_TO_ALL_BIT(TASK_NOTIF_DETECT_DONE),
                               pdTRUE,
                               pdTRUE,
                               timeout);

    if ((bits & NOTIF_TO_ALL_BIT(TASK_NOTIF_DETECT_DONE)) != bits) {
        ESP_LOGE(TAG, "error in event bits TASK_NOTIF_READ_DONE");
    }

    PRINT_TASK_HIGHWATER_MARK(NULL);
}

static void __scheduled_suspend_phase(TickType_t timeout)
{
    EventBits_t bits;

    ESP_LOGI(TAG, "SUSPEND Phase!");
    xEventGroupSetBits(__sensormng.evt_grp, NOTIF_TO_ALL_BIT(TASK_NOTIF_SUSPEND));

    bits = xEventGroupWaitBits(__sensormng.evt_grp,
                               NOTIF_TO_ALL_BIT(TASK_NOTIF_SUSPEND_DONE),
                               pdTRUE,
                               pdTRUE,
                               timeout);

    PRINT_TASK_HIGHWATER_MARK(NULL);
    if ((bits & NOTIF_TO_ALL_BIT(TASK_NOTIF_SUSPEND_DONE)) != bits) {
        ESP_LOGE(TAG, "error in event bits TASK_NOTIF_READ_DONE");
    }

    PRINT_TASK_HIGHWATER_MARK(NULL);
}


static void __scheduled_enable_all_sensor() {

    sensor_inst_t *sensor_inst;
    for (can5_phy_io_type_t type = 0; type < CAN5_PHY_IO_TYPE_COUNT; type++) {

        // iterate over ready sensors
        TAILQ_FOREACH(sensor_inst, &S_TYPE(type)->undetected, te) {
            ESP_LOGI(TAG, "schedule enable all sensors ENABLE sensor %s on port %s",
                     can5_sensor_type_getstr(sensor_inst->sensordriv->details.sensor.type),
                     can5_hal_port_getstr(sensor_inst->port));
            hal.enable(true, sensor_inst->port);
        }
    }
}

static void __scheduled_cycle()
{
    int64_t cycle_time_ms;
    int64_t read_time_ms, sleep_time_ms, remaining_time_ms;
    EventBits_t bits;
    sensor_inst_t *max_sensor_inst;

    cycle_time_ms = can5_sec_to_ms(__get_cycle_period());

    bits = xEventGroupClearBits(__sensormng.evt_grp, NOTIF_BITS_ALL);

    ESP_LOGI(TAG, "Scheduled cycle, clear bits: %xl", bits);

    // schedule enable also has calls to __sleep_until
    __scheduled_enable();

    // start of our read phase
    max_sensor_inst = NULL;
    for (can5_phy_io_type_t type = 0; type < CAN5_PHY_IO_TYPE_COUNT; type++) {
        S_LIST_MAX(type, ready, sensordriv->details.sensor.read_time, max_sensor_inst);
    }

    if (!max_sensor_inst) {
        ESP_LOGE(TAG, "Sensors not found!");
        read_time_ms = can5_time_ms(NULL);
    } else {
        ESP_LOGI(TAG, "%s sensor has read time %ld",
                 can5_sensor_type_getstr(max_sensor_inst->sensordriv->details.sensor.type),
                 max_sensor_inst->sensordriv->details.sensor.read_time);
        // deduct max warm up from the remaining time to wake up to enable
        read_time_ms = can5_time_ms(NULL);

        // reset the cycle data
        __scheduled_reset_cycle_data();

        ESP_LOGE(TAG, "READ time %lld", can5_ms_to_sec(read_time_ms));
        __scheduled_read_phase(can5_sec_to_ms(10)); // figure out proper number

        time_t sensor_read_time = max_sensor_inst->sensordriv->details.sensor.read_time;
        if (sensor_read_time == CAN5_SENSOR_TIME_MAX) {
            sensor_read_time = can5_ms_to_sec(cycle_time_ms);
        }
        else {
            // wait for 5 seconds more than expected finish
            sensor_read_time += 5;
        }

        __scheduled_run_phase(can5_sec_to_ms(sensor_read_time));

        // store data in sd card
        if (!TAILQ_EMPTY(&S_UART->ready) || !TAILQ_EMPTY(&S_I2C->ready)
#if CONFIG_CAN5_SENSORMNG_ADC_TASK_ENABLE
        || !TAILQ_EMPTY(&S_ADC->ready)
#endif
        ) {
            __save_data_to_storage();
        }
    }


    // try to detect until retires run over
    __scheduled_detect_phase(portMAX_DELAY);


    // TODO: ?
    __scheduled_suspend_phase(portMAX_DELAY);
    // start of our idle phase

    max_sensor_inst = NULL;
    for (can5_phy_io_type_t type = 0; type < CAN5_PHY_IO_TYPE_COUNT; type++) {
        S_LIST_MAX(type, ready, sensordriv->details.sensor.read_warm_up, max_sensor_inst);
    }

    sleep_time_ms = can5_time_ms(NULL);
    // remainder of the time
    remaining_time_ms = cycle_time_ms - (sleep_time_ms - read_time_ms);

    if (!max_sensor_inst) {
        ESP_LOGE_V(TAG, "sensor with max warm up time not found!");
    } else {
        ESP_LOGI_V(TAG, "%s sensor has warm up time %ld",
                 can5_sensor_type_getstr(max_sensor_inst->sensordriv->details.type),
                 max_sensor_inst->sensordriv->details.read_warm_up);
        // deduct max warm up from the remaining time to wake up to enable
        remaining_time_ms -= can5_sec_to_ms(max_sensor_inst->sensordriv->details.sensor.read_warm_up);
    }

    remaining_time_ms = remaining_time_ms < 0? 0: remaining_time_ms;

    ESP_LOGI(TAG, "Sleeping for %lld ms.", remaining_time_ms);



    __sleep_until(remaining_time_ms);

    vTaskDelay(pdMS_TO_TICKS(remaining_time_ms));

}

static void __schedule_realtime()
{
    int64_t cycle_time_ms;
    EventBits_t bits;
    TickType_t frequency;
    TickType_t prev_wake_up;

    frequency = pdMS_TO_TICKS(can5_sec_to_ms(__get_cycle_period()));

    cycle_time_ms = can5_sec_to_ms(__get_cycle_period());

    bits = xEventGroupClearBits(__sensormng.evt_grp, NOTIF_BITS_ALL);

    ESP_LOGI(TAG, "Scheduled cycle, clear bits: %xl", bits);

        // deduct max warm up from the remaining time to wake up to enable
    prev_wake_up = xTaskGetTickCount();
    // reset the cycle data
    __scheduled_reset_cycle_data();
    // schedule enable also has calls to __sleep_until
    __scheduled_read_phase(cycle_time_ms); // figure out proper number
    __scheduled_run_phase(cycle_time_ms);
    // store data in sd card
    if (!TAILQ_EMPTY(&S_UART->ready) || !TAILQ_EMPTY(&S_I2C->ready)
#if CONFIG_CAN5_SENSORMNG_ADC_TASK_ENABLE
        || !TAILQ_EMPTY(&S_ADC->ready)
#endif
        ) {
        __save_data_to_storage();
    }

    // try to detect until retires run over
    __scheduled_detect_phase(portMAX_DELAY);

    // TODO: ?
    __scheduled_suspend_phase(portMAX_DELAY);

    // start of our idle phase
    xTaskDelayUntil(&prev_wake_up, frequency);
}


portTASK_FUNCTION(__task_sensormng, pv)
{
    char *data_mode;

    data_mode = NULL;
    allocate_params_t params = {
        .is_high_freq_imu = false,
        .is_realtime = false
    };

    if(config_manager.read(CFG_DEVICE_DATA_MODE, (uint8_t **)&data_mode, NULL) == CAN5_SUCCESS) {
        params.is_realtime = strcmp(data_mode, can5_device_data_mode_get_str(CAN5_DATA_MODE_REALTIME)) == 0;
    }

#if CONFIG_CAN5_HIGH_FREQ_IMU_ENABLE
    if(config_manager.read_bool(CFG_HIGH_FREQ_IMU, &params.is_high_freq_imu) != CAN5_SUCCESS) {
        ESP_LOGE(TAG, "CFG_HIGH_FREQ_IMU could not be read. Defaulting to false.");
    }
#endif

    ESP_LOGI(TAG, "Data Mode: %s is_realtime: %s high_freq_imu: %s", data_mode, boolean_get_str(params.is_realtime),
             boolean_get_str(params.is_high_freq_imu));

    if (data_mode) {
        free(data_mode);
    }

    CAN5_ERR_CHECK(__allocate_sensor(&params));
    CAN5_ERR_CHECK(__allocate_actuator(params.is_realtime));

#if CONFIG_CAN5_HIGH_FREQ_IMU_ENABLE
    if (params.is_high_freq_imu && params.out_imu_sensor_inst) {
        if ((__sensormng.high_freq.task.hdl = xTaskCreateStatic(__task_high_freq_imu,
                                                                "high_freq_imu",
                                                                CONFIG_CAN5_SENSORMNG_HIGH_FREQ_IMU_TASK_STACK_SIZE,
                                                                params.out_imu_sensor_inst,
                                                                CONFIG_CAN5_SENSORMNG_HIGH_FREQ_IMU_TASK_PRIORITY,
                                                                task_stacks.high_freq,
                                                                &__sensormng.high_freq.task.buffer)) == NULL) {
            ESP_LOGI(TAG, "Cannot start high frequency task.");
        }

    }
#endif

    if (params.is_realtime) {
        __scheduled_enable_all_sensor();
    }
    __scheduled_detect_phase(portMAX_DELAY);

    for (;;) {
        /* There are two modes, either realtime or scheduled read */
        if (params.is_realtime) {
            __schedule_realtime();
        }
        else {
            __scheduled_cycle(params.is_realtime);
        }
    }
}

#if CONFIG_CAN5_HIGH_FREQ_IMU_ENABLE

typedef enum imu_state_e {
    IMU_UNINIT,
    IMU_DETECTED,
    IMU_READY,
} imu_state_t;

portTASK_FUNCTION(__task_high_freq_imu, pv)
{
    sensor_inst_t *imu = (sensor_inst_t *)pv;
    can5_err_t ret;
    TickType_t frequency;
    int64_t rate;
    TickType_t prev_wake_up;

    HIGH_FREQ_IMU->buffer_len = 0;
    CLEAR_ARRAY(HIGH_FREQ_IMU->buffer);

    if(config_manager.read_int(CFG_HIGH_FREQ_IMU_FREQ, &rate) != CAN5_SUCCESS) {
        ESP_LOGE(TAG, "CFG_HIGH_FREQ_IMU_FREQ could not be read. Defaulting to 20 Hz.");
        rate = 20;
    }



    imu_state_t  state = IMU_UNINIT;
    frequency = pdMS_TO_TICKS(1000/rate);

    ESP_LOGI(TAG,"High frequency IMU started! With per cycle tick %d", frequency);


    for(;;) {
        prev_wake_up = xTaskGetTickCount();

        switch (state) {

            case IMU_UNINIT:
                ret = imu->sensordriv->ops.detect(imu->hdl, imu->port);
                if (ret == CAN5_SUCCESS) {
                    state = IMU_DETECTED;
                    ESP_LOGI(TAG,"High frequency IMU detected!");
                }
                else {
                    CAN5_ERR_CHECK_NO_ABORT(ret);
                    vTaskDelay(pdMS_TO_TICKS(5000));
                }
                break;
            case IMU_DETECTED:
                ret = imu->sensordriv->ops.init(imu->hdl, imu->port, NULL);
                if (ret == CAN5_SUCCESS) {
                    state = IMU_READY;
                    ESP_LOGI(TAG,"High frequency IMU Ready!");
                }
                else {
                    CAN5_ERR_CHECK_NO_ABORT(ret);
                    vTaskDelay(pdMS_TO_TICKS(5000));
                }
                break;
            case IMU_READY: {
                can5_sensor_mpu6500_data_t data;
                ret = imu->sensordriv->ops.read(imu->hdl, &data, NULL, pdMS_TO_TICKS(5), true);

                CAN5_ERR_CHECK_NO_ABORT(ret);
                if (ret == CAN5_SUCCESS) {
                    if (HIGH_FREQ_IMU->buffer_len + CAN5_SENSOR_PARSED_DATA_MAX_LEN + 1>= CONFIG_CAN5_HIGH_FREQ_IMU_BUFFER_SIZE) {
                        CAN5_ERR_CHECK_NO_ABORT(can5_storage_push_fs(IMU_DATA_TAG, HIGH_FREQ_IMU->buffer, HIGH_FREQ_IMU->buffer_len - 1));
                        CLEAR_ARRAY(HIGH_FREQ_IMU->buffer);
                        HIGH_FREQ_IMU->buffer_len = 0;
                    }

                    HIGH_FREQ_IMU->buffer_len += parse_mpu6500_to_csv(&data, time_ms(NULL),
                                                                      (char *)&HIGH_FREQ_IMU->buffer[HIGH_FREQ_IMU->buffer_len]);   // remove 1 for null character

                    HIGH_FREQ_IMU->buffer[HIGH_FREQ_IMU->buffer_len++] = '\n';
                }

            }
        }
        xTaskDelayUntil(&prev_wake_up, frequency);
    }
}

#endif
/* ---------------------------------------------------------------------
 * Debug Support
 -----------------------------------------------------------------------*/

#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL > 0

/**
 * @brief RTC status tags
 *
 */
static const can5_tag_tab_t _sensor_stat_tags = {
    TAG_TAB_ITEM(SENSORMNG_STAT_UNINITD),
    TAG_TAB_ITEM(SENSORMNG_STAT_INITD),
};


static const char *status_getstr(int32_t status)
{

    return TAG_LOOKUP(status, _sensor_stat_tags);
}

static const char *evt_getstr(int32_t evt)
{

    return "RTC EVT";
}

static const can5_tag_tab_t _sensor_hwevt_type_tags = {
    TAG_TAB_ITEM(CAN5_SENSOR_HWEVT_NONE),
    TAG_TAB_ITEM(CAN5_SENSOR_HWEVT_UNIMPLIMENTED),
    TAG_TAB_ITEM(CAN5_SENSOR_HWEVT_READING),
    TAG_TAB_ITEM(CAN5_SENSOR_HWEVT_READ_FAILURE),
    TAG_TAB_ITEM(CAN5_SENSOR_HWEVT_READ_COMPLETE),
};

const char *sensor_hwevt_type_getstr(int type)
{
    return TAG_LOOKUP(type, _sensor_hwevt_type_tags);
}

#endif
