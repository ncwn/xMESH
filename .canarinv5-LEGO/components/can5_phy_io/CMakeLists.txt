set(srcdirs "src")
set(includedirs "include")
set(requires can5_common can5_hal can5_storage can5_rtc can5_net can5_config libnmea-esp32)
set(excludesrcs "")

if (CONFIG_CAN5_SENSOR_BUILD_TESTS)
    message(STATUS "Compiling Sensor Tests")
    list(APPEND srcdirs "test")
    list(APPEND includedirs "test")
endif()

# sensors
list(APPEND srcdirs "can5_sensor/can5_sensor_ze07_co_adc")
list(APPEND includedirs "can5_sensor/can5_sensor_ze07_co_adc")
list(APPEND srcdirs "can5_sensor/can5_sensor_ublox_neo_gps")
list(APPEND includedirs "can5_sensor/can5_sensor_ublox_neo_gps")
list(APPEND srcdirs "can5_sensor/can5_sensor_ublox_neo_gps/ubxlib")
list(APPEND includedirs "can5_sensor/can5_sensor_ublox_neo_gps/ubxlib")
list(APPEND srcdirs "can5_sensor/can5_sensor_mh_z16_co2")
list(APPEND includedirs "can5_sensor/can5_sensor_mh_z16_co2")
list(APPEND srcdirs "can5_sensor/can5_sensor_bme280")
list(APPEND includedirs "can5_sensor/can5_sensor_bme280")
list(APPEND srcdirs "can5_sensor/can5_sensor_ze03_no2")
list(APPEND includedirs "can5_sensor/can5_sensor_ze03_no2")
list(APPEND srcdirs "can5_sensor/can5_sensor_ze07_co")
list(APPEND includedirs "can5_sensor/can5_sensor_ze07_co")
list(APPEND srcdirs "can5_sensor/can5_sensor_pms7003")
list(APPEND includedirs "can5_sensor/can5_sensor_pms7003")
list(APPEND srcdirs "can5_sensor/can5_sensor_sim7600_gps")
list(APPEND includedirs "can5_sensor/can5_sensor_sim7600_gps")
list(APPEND srcdirs "can5_sensor/can5_sensor_ws3226")
list(APPEND includedirs "can5_sensor/can5_sensor_ws3226")
list(APPEND srcdirs "can5_sensor/can5_sensor_mpu6500")
list(APPEND includedirs "can5_sensor/can5_sensor_mpu6500")
list(APPEND srcdirs "can5_sensor/can5_sensor_scd41")
list(APPEND includedirs "can5_sensor/can5_sensor_scd41")
list(APPEND srcdirs "can5_sensor/can5_sensor_scd41/embedded-i2c-scd4x")
list(APPEND includedirs "can5_sensor/can5_sensor_scd41/embedded-i2c-scd4x")
list(APPEND excludesrcs "can5_sensor/can5_sensor_scd41/embedded-i2c-scd4x/scd4x_i2c_example_usage.c" "can5_sensor/can5_sensor_scd41/embedded-i2c-scd4x/sensirion_i2c_hal.c")

# actuators
list(APPEND srcdirs "can5_actuator/can5_actuator_relay")
list(APPEND includedirs "can5_actuator/can5_actuator_relay")
# idf_component_register([[SRCS src1 src2 ...] | [[SRC_DIRS dir1 dir2 ...] [EXCLUDE_SRCS src1 src2 ...]]

idf_component_register(SRC_DIRS ${srcdirs}
        EXCLUDE_SRCS ${excludesrcs}
        INCLUDE_DIRS ${includedirs}
        #    PRIV_INCLUDE_DIRS dir1 dir2
        REQUIRES ${requires}
        #    PRIV_REQUIRES component1 component2
        #    LDFRAGMENTS ldfragment1 ldfragment2
        #    REQUIRED_IDF_TARGETS target1 target2
        #    EMBED_FILES file1 file2
        #    EMBED_TXTFILES file1 file2
        #    KCONFIG kconfig
        #    KCONFIG_PROJBUILD kconfig_projbuild
        )


target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=unused-variable -Werror=int-conversion)