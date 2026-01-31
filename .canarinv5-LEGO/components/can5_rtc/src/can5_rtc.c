/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   12/01/22
*
* File:  can5_rtc.cpp
* Descr:
*******************************************************************************/
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "sys/time.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "can5_utils.h"
#include "can5_rtc.h"
#include "can5_hal.h"
#include "esp_log.h"

#define ISL1219_ADDR                0x6F
#define SR_REG                      0x07
#define SR_REG_LEN                  1
#define INT_REG                     0x08
#define INT_REG_LEN                 1
#define RTC_REG                     0x00
#define RTC_REG_LEN                 6
#define ALRM_REG                    0x0C
#define ALRM_REG_LEN                5

/* CtrlREG
 * ARST, XTOSCB, Reservered, WRTC, EVT, ALM, BAT, RTCF */
#define SR_REG_RTCF_BIT             (1 << 0)
#define SR_REG_WRTC_BIT             (1 << 4)
#define SR_REG_ARST_BIT             (1 << 7)
#define SR_REG_INIT_BITS            (SR_REG_ARST_BIT | SR_REG_WRTC_BIT)
#define INT_REG_ALME_BIT            (1 << 6)
#define INT_REG_IM_BIT              (1 << 7)
#define RTC_HR_MIL_BIT              (1 << 7)
#define ENABLE_ALRM_MSB             (1 << 7)

#define CHECK_BITS_SET(reg, bits)   ((reg & bits) == bits)

#define DRIFT_THRESHOLD_USEC         250000

static const char *TAG = "RTC";

#if 1
#define TRACE_FUNC_RTC ESP_LOGI(TAG, "in -> %s() :%d", __FUNCTION__, __LINE__)
#else
#define TRACE_FUNC_RTC
#endif

static const struct tm default_rtc_tm = {
    .tm_sec = 0,
    .tm_min = 0,
    .tm_hour = 0,
    .tm_mday = 1,
    .tm_mon = 0,
    .tm_year = 2017 - 1900,
};

typedef enum can5_rtc_status_e {
    RTC_STAT_UNINITD = 0,
    RTC_STAT_INITD,

    RTC_STAT_LAST,
} can5_rtc_status_t;

/**
 * @brief Bytes to hold time structure in RTC
 */

typedef struct can5_time_s {
    uint8_t sec;
    uint8_t min;
    uint8_t hour;
#if CONFIG_IDF_TARGET_ESP32S3
    uint8_t wday;
#endif
    uint8_t date;
    uint8_t month;
    uint8_t year;
} __attribute__((packed)) can5_time_t;

static can5_err_t init();

static can5_err_t uninit();

static can5_err_t run();

static can5_err_t pre_sleep();

static can5_err_t post_sleep();

static bool is_sleepable();

static bool is_sleeping();

static can5_err_t register_hook(can5_rtc_hook_type_t hook_type, can5_rtc_hook_t hook_cb);

static int32_t status_get();

#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL > 0

static const char *status_getstr(int32_t status);

static const char *evt_getstr(int32_t evt);

#endif

static can5_err_t get_time(struct tm *tm);

static can5_err_t set_time(const struct tm *tm);

static can5_err_t rtc_to_sys();

static can5_err_t sys_to_rtc();

static can5_err_t set_alarm(const struct tm *tm);

static int64_t get_current_uptime_usec();

static can5_err_t adjust_gps_time(struct tm *tm, int64_t acquired_usec);

static can5_err_t print_rtc_time();

static can5_err_t reset_to_default_time();

static can5_err_t can5_sleep();

can5_rtc_t rtc = {
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
        .get_time = get_time,
        .set_time = set_time,
        .rtc_to_sys = rtc_to_sys,
        .sys_to_rtc = sys_to_rtc,
        .set_alarm = set_alarm,
        .sleep = can5_sleep,
        .register_hook = register_hook,
        .get_current_uptime_usec = get_current_uptime_usec,
        .adjust_gps_time = adjust_gps_time,
        .print_rtc_time = print_rtc_time,
        .reset_to_default_time = reset_to_default_time,
};

static struct rtc_hdl_s {
    volatile can5_rtc_status_t status;
    can5_rtc_hook_t hooks[CAN5_RTC_HOOKS_COUNT];
    time_t offset_sec;      /* offset sec to account for processing time */
} __rtc = {
        .status = RTC_STAT_UNINITD,
        .hooks = { NULL },
        .offset_sec = 0,
};

/****************************************
 * Private forward declarations
 ****************************************/

uint8_t bcd_to_dec(uint8_t bcd) {
    uint8_t dec = bcd & 0x0F;
    dec += ((bcd & 0xF0) >> 4) * 10;
    return dec;
}

uint8_t dec_to_bcd(uint8_t dec) {
    uint8_t bcd = ((dec % 10) & 0x0F);
    bcd |= (((dec / 10) << 4) & 0xF0);
    return bcd;
}

static struct tm rtc_to_tm(can5_time_t *rtc_time) {
    struct tm tm;
    tm.tm_sec = bcd_to_dec(rtc_time->sec);
    tm.tm_min = bcd_to_dec(rtc_time->min);
#if CONFIG_IDF_TARGET_ESP32
    tm.tm_hour = bcd_to_dec(rtc_time->hour & ~RTC_HR_MIL_BIT); // Remove MIL (7th bit)
    tm.tm_mon = bcd_to_dec(rtc_time->month) - 1;            // RTC: 1:12, POSIX 0:11
#elif CONFIG_IDF_TARGET_ESP32S3
    tm.tm_hour = bcd_to_dec(rtc_time->hour & 0x3f); // Remove 12/24 (6th bit)
    tm.tm_mon = bcd_to_dec(rtc_time->month & 0x7f) - 1;            // RTC: 1:12, POSIX 0:11
#endif
    tm.tm_mday = bcd_to_dec(rtc_time->date);
    tm.tm_year = bcd_to_dec(rtc_time->year) + 100;          // RTC: 2000+, POSIX 1900
    tm.tm_isdst = 0;

    return tm;
}

static can5_time_t tm_to_rtc(const struct tm *tm) {
    can5_time_t rtc_time;
    rtc_time.sec = dec_to_bcd(tm->tm_sec);
    rtc_time.min = dec_to_bcd(tm->tm_min);
#if CONFIG_IDF_TARGET_ESP32
    rtc_time.hour = dec_to_bcd(tm->tm_hour) | RTC_HR_MIL_BIT; // Add MIL (7th bit, 24 hour clock)
    rtc_time.month = dec_to_bcd(tm->tm_mon + 1);
#elif CONFIG_IDF_TARGET_ESP32S3
    rtc_time.hour = dec_to_bcd(tm->tm_hour);
    rtc_time.month = dec_to_bcd(tm->tm_mon + 1);
#endif
    rtc_time.date = dec_to_bcd(tm->tm_mday);
    rtc_time.year = dec_to_bcd(tm->tm_year - 100);
    return rtc_time;
}

static can5_err_t init() {
    TRACE_FUNC_RTC;

#if CONFIG_IDF_TARGET_ESP32
    uint8_t ctrl_reg;
    ctrl_reg = 0;
    VERIFY_SUCCESS(hal.i2c_read_reg(ISL1219_ADDR, INT_REG, &ctrl_reg, INT_REG_LEN, I2C_MASTER_LAST_NACK));
    ESP_LOGI(TAG, "RTC INT reg: 0x%x", ctrl_reg);
    ESP_LOGI(TAG, "Reset RTC INT reg: 0x00");
    ctrl_reg = 0;
    VERIFY_SUCCESS(hal.i2c_write_reg(ISL1219_ADDR, INT_REG, &ctrl_reg, INT_REG_LEN, true));

    VERIFY_SUCCESS(hal.i2c_read_reg(ISL1219_ADDR, 0x09, &ctrl_reg, SR_REG_LEN, I2C_MASTER_LAST_NACK));
    ESP_LOGI(TAG, "RTC EV reg: 0x%x", ctrl_reg);
    VERIFY_SUCCESS(hal.i2c_read_reg(ISL1219_ADDR, 0x0A, &ctrl_reg, SR_REG_LEN, I2C_MASTER_LAST_NACK));
    ESP_LOGI(TAG, "RTC ATR reg: 0x%x", ctrl_reg);
    VERIFY_SUCCESS(hal.i2c_read_reg(ISL1219_ADDR, 0x0B, &ctrl_reg, SR_REG_LEN, I2C_MASTER_LAST_NACK));
    ESP_LOGI(TAG, "RTC DTR reg: 0x%x", ctrl_reg);
    VERIFY_SUCCESS(hal.i2c_read_reg(ISL1219_ADDR, SR_REG, &ctrl_reg, SR_REG_LEN, I2C_MASTER_LAST_NACK));
    ESP_LOGI(TAG, "RTC SR reg: 0x%x", ctrl_reg);

    if (CHECK_BITS_SET(ctrl_reg,SR_REG_RTCF_BIT) ||  !CHECK_BITS_SET(ctrl_reg, SR_REG_INIT_BITS)) {
        /* Check if RTC has total hardware power down or if the SR bits are not set correctly */
        ctrl_reg = SR_REG_INIT_BITS;
        // Set sys time to RTC time
        // Maybe 2017
        ESP_LOGI(TAG, "Reset RTC INT reg: 0x%x", ctrl_reg);
        VERIFY_SUCCESS(hal.i2c_write_reg(ISL1219_ADDR, SR_REG, &ctrl_reg, SR_REG_LEN, true));
        VERIFY_SUCCESS(reset_to_default_time());
    }
#elif CONFIG_IDF_TARGET_ESP32S3
    uint8_t ctrl_reg;
    ctrl_reg = 0;
    VERIFY_SUCCESS(hal.i2c_read_reg(0x68, 0x0F, &ctrl_reg, INT_REG_LEN, I2C_MASTER_LAST_NACK));
    ESP_LOGI(TAG, "RTC INT reg: 0x%x", ctrl_reg);
    ESP_LOGI(TAG, "Reset RTC INT reg: 0x00");
#endif

    VERIFY_SUCCESS(rtc.rtc_to_sys());

    //VERIFY_SUCCESS(gpio_wakeup_enable(CAN5_PIN_RTCINT, GPIO_INTR_LOW_LEVEL));
    //VERIFY_SUCCESS(esp_sleep_enable_gpio_wakeup());
    __rtc.status = RTC_STAT_INITD;

    return CAN5_SUCCESS;
}

static can5_err_t uninit() {
    TRACE_FUNC_RTC;
    return CAN5_SUCCESS;
}

static can5_err_t run() {
    TRACE_FUNC_RTC;

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
    return false;
}

static bool is_sleeping()
{
    return false;
}

static int32_t status_get() {
    TRACE_FUNC_RTC;

    return __rtc.status;
}

static can5_err_t get_time(struct tm *tm) {
    TRACE_FUNC_RTC;

    can5_time_t rtc_time;
    VERIFY_NOT_NULL(tm);

#if CONFIG_IDF_TARGET_ESP32
    VERIFY_SUCCESS(hal.i2c_read_reg(ISL1219_ADDR,
                                    RTC_REG,
                                    (uint8_t *) &rtc_time,
                                    RTC_REG_LEN,
                                    I2C_MASTER_LAST_NACK));
#elif CONFIG_IDF_TARGET_ESP32S3
    VERIFY_SUCCESS(hal.i2c_read_reg(0x68,
                                    RTC_REG,
                                    (uint8_t *) &rtc_time,
                                    sizeof (can5_time_t),
                                    I2C_MASTER_LAST_NACK));
#endif
    *tm = rtc_to_tm(&rtc_time);

    // Adjust the remaining members in tm
    if (mktime(tm) == -1) {
        return CAN5_TIME_ERR_MKTIME;
    }

    return CAN5_SUCCESS;
}

static can5_err_t set_time(const struct tm *tm) {
    TRACE_FUNC_RTC;

    can5_time_t rtc_time;
    VERIFY_NOT_NULL(tm);

    rtc_time = tm_to_rtc(tm);

#if CONFIG_IDF_TARGET_ESP32
    VERIFY_SUCCESS(hal.i2c_write_reg(ISL1219_ADDR,
                                     RTC_REG,
                                     (uint8_t *) &rtc_time,
                                     RTC_REG_LEN,
                                     true));
#elif CONFIG_IDF_TARGET_ESP32S3
    VERIFY_SUCCESS(hal.i2c_write_reg(0x68,
                                     RTC_REG,
                                     (uint8_t *) &rtc_time,
                                     sizeof (can5_time_t),
                                     true));
#endif

    return CAN5_SUCCESS;
}

static can5_err_t rtc_to_sys() {
    TRACE_FUNC_RTC;
    struct tm tm;
    VERIFY_SUCCESS(rtc.get_time(&tm));
    time_t t1;

    t1 = mktime(&tm);
    if (t1 == -1) {
        return CAN5_TIME_ERR_MKTIME;
    }

    struct timeval tv = {
            .tv_sec = t1,
            .tv_usec = 0,
    };

    if (settimeofday(&tv, NULL) != 0) {
        return CAN5_TIME_ERR_SET_SYS_TIME;
    }

    return CAN5_SUCCESS;
}

static can5_err_t sys_to_rtc() {
    TRACE_FUNC_RTC;
    struct timeval tv;
    int64_t diff_usec;
    struct tm tm;
    if (gettimeofday(&tv, NULL) != 0) {
        return CAN5_TIME_ERR_GET_SYS_TIME;
    }

    diff_usec = 1000000 - tv.tv_usec;
    // calculate offset from next sec update

    vTaskDelay(pdMS_TO_TICKS(USEC_TO_MS(diff_usec)));

    /* Waited for the difference, update sec */
    tv.tv_sec += 1;

    tm = *gmtime(&tv.tv_sec);
    VERIFY_SUCCESS(rtc.set_time(&tm));

    if (__rtc.hooks[CAN5_RTC_HOOKS_TIME_UPDATE]) {
        return __rtc.hooks[CAN5_RTC_HOOKS_TIME_UPDATE]();
    }

    return CAN5_SUCCESS;
}

static can5_err_t can5_sleep()
{
    TRACE_FUNC_RTC;
    uint8_t int_reg;

    /* TODO: Hack */
    vTaskDelay(pdMS_TO_TICKS(2000));
    if (__rtc.hooks[CAN5_RTC_HOOKS_PRE_SLEEP]) {
        VERIFY_SUCCESS(__rtc.hooks[CAN5_RTC_HOOKS_PRE_SLEEP]());
    }

    VERIFY_SUCCESS(esp_light_sleep_start());

    // Clear INT register, to remove ALME and INT bits
    int_reg = 0x00;

    VERIFY_SUCCESS(hal.i2c_write_reg(ISL1219_ADDR,
                                     INT_REG,
                                     &int_reg,
                                     INT_REG_LEN,
                                     true));

    if (__rtc.hooks[CAN5_RTC_HOOKS_POST_SLEEP]) {
        VERIFY_SUCCESS(__rtc.hooks[CAN5_RTC_HOOKS_POST_SLEEP]());
    }

    return CAN5_SUCCESS;
}

static can5_err_t set_alarm(const struct tm *tm) {
    TRACE_FUNC_RTC;
    can5_time_t rtc_time;
    uint8_t int_reg;

    rtc_time = tm_to_rtc(tm);
    /* set msb of sec, minute, hour, day and month */

    rtc_time.sec |= ENABLE_ALRM_MSB;
    rtc_time.min |= ENABLE_ALRM_MSB;
    rtc_time.hour |= ENABLE_ALRM_MSB;
    rtc_time.date |= ENABLE_ALRM_MSB;
    rtc_time.month |= ENABLE_ALRM_MSB;

    /* Set the Alarm time */
    VERIFY_SUCCESS(hal.i2c_write_reg(ISL1219_ADDR,
                                     ALRM_REG,
                                     (uint8_t *) &rtc_time,
                                     ALRM_REG_LEN,
                                     true));

    /* Enable interrupt mode alarm enable to get 250ms pulse */
    int_reg = INT_REG_ALME_BIT| INT_REG_IM_BIT;

    VERIFY_SUCCESS(hal.i2c_write_reg(ISL1219_ADDR,
                                     INT_REG,
                                     &int_reg,
                                     INT_REG_LEN,
                                     true));

    return CAN5_SUCCESS;
}

static can5_err_t register_hook(can5_rtc_hook_type_t hook_type, can5_rtc_hook_t hook_cb)
{
    TRACE_FUNC_RTC;
    VERIFY_NOT_NULL(hook_cb);

    __rtc.hooks[hook_type] = hook_cb;

    return CAN5_SUCCESS;
}

static int64_t get_current_uptime_usec()
{
    int64_t uptime = esp_timer_get_time();
    return uptime;
}

static can5_err_t adjust_gps_time(struct tm *tm, int64_t acquired_usec)
{
    TRACE_FUNC_RTC;
    struct timeval gps_tv, sys_tv, diff_tv;
    int64_t now, diff_usec;

    now = get_current_uptime_usec();
    gps_tv.tv_usec = now - acquired_usec;
    if ((gps_tv.tv_sec = mktime(tm)) == -1) {
        return CAN5_TIME_ERR_MKTIME;
    }
    if (gettimeofday(&sys_tv, NULL) != 0) {
        return CAN5_TIME_ERR_GET_SYS_TIME;
    }

    ESP_LOGI(TAG, "GPS time %s", asctime(tm));

    timersub(&gps_tv, &sys_tv, &diff_tv);

    if (diff_tv.tv_sec > 100) {
        /* Avoid overflow  */
        diff_tv.tv_sec = 100;
    }
    diff_usec = diff_tv.tv_sec * 1000000 + diff_tv.tv_usec;
    if (diff_usec < 0) {
        diff_usec += -1;
    }

    if (diff_usec < DRIFT_THRESHOLD_USEC) {
        /* Our systime is still synced */
        return CAN5_SUCCESS;
    }
    ESP_LOGI(TAG, "gpsts %ld systs %ld diff %ld %ld diff_usec %lld", gps_tv.tv_sec, sys_tv.tv_sec, diff_tv.tv_sec, diff_tv.tv_usec, diff_usec);

    now = get_current_uptime_usec();
    sys_tv.tv_sec = gps_tv.tv_sec;
    diff_usec = now - acquired_usec;
    if (diff_usec >= 1000000) {
        sys_tv.tv_usec = diff_usec - 1000000;
        sys_tv.tv_sec += 1;
    }

    if (settimeofday(&sys_tv, NULL) != 0) {
        return CAN5_TIME_ERR_SET_SYS_TIME;
    }

    /* wait for second update */
    diff_usec = 1000000 - sys_tv.tv_usec;

    vTaskDelay(pdMS_TO_TICKS(USEC_TO_MS(diff_usec)));

    VERIFY_SUCCESS(sys_to_rtc());

    return CAN5_SUCCESS;
}

static can5_err_t print_rtc_time()
{
    TRACE_FUNC_RTC;
    struct tm tm;
    VERIFY_SUCCESS(get_time(&tm));
    ESP_LOGI(TAG, "RTC time: %s", asctime(&tm));
    return CAN5_SUCCESS;
}

static can5_err_t  reset_to_default_time()
{
    return rtc.set_time(&default_rtc_tm);
}

#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL > 0

/**
 * @brief RTC status tags
 *
 */
static const can5_tag_tab_t _rtc_stat_tags = {
        TAG_TAB_ITEM(RTC_STAT_UNINITD),
        TAG_TAB_ITEM(RTC_STAT_INITD)
};


static const char *status_getstr(int32_t status) {
    TRACE_FUNC_RTC;

    return TAG_LOOKUP(status, _rtc_stat_tags);
}

static const char *evt_getstr(int32_t evt) {
    TRACE_FUNC_RTC;

    return "RTC EVT";
}

#endif
