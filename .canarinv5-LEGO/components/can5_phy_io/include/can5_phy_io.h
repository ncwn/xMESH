/**************************************************
 * Author: rmukhia
 * Creation Date: 30/9/22
 * Description: 
 **************************************************/

#ifndef CANARINV5_LEGO_CAN5_PHY_IO_H
#define CANARINV5_LEGO_CAN5_PHY_IO_H

#define CAN5_DEVVER_LEN             7
#define CAN5_DEVNAM_LEN             25
#define CAN5_DEVMAN_LEN             25
#define CAN5_DEVHWINFO_LEN          255

typedef enum can5_phy_io_type_e {
    CAN5_PHY_IO_TYPE_ADC = 0,
    CAN5_PHY_IO_TYPE_I2C,
    CAN5_PHY_IO_TYPE_UART,

    CAN5_PHY_IO_TYPE_COUNT,
} can5_phy_io_type_t;

typedef struct can5_phy_io_details_s {
    const can5_phy_io_type_t io_type;
    union {
        struct {
            const can5_sensordriv_type_t type;
            const time_t init_warm_up;               // initialization warm up
            const time_t read_warm_up;               // read warm up
            const time_t read_time;                  // time to read
        } sensor;
        struct {
            const can5_actuatordriv_type_t type;
        } actuator;
    };
    const char version[CAN5_DEVVER_LEN+1];   // add one more byte for termination character
    const char name[CAN5_DEVNAM_LEN+1];      // add one more byte for termination character
    const char manufacturer[CAN5_DEVMAN_LEN+1];  // add one more byte for termination character
    char hwinfo[CAN5_DEVHWINFO_LEN+1];  // add one more byte for termination character
} can5_phy_io_details_t;

#endif //CANARINV5_LEGO_CAN5_PHY_IO_H
