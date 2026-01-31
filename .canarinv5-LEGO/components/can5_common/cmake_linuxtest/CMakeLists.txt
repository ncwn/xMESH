# For linux_host_test

set(SOURCE
        ${SOURCE}
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/can5_error.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/can5_sensor_data.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/can5_utils.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/can5_types.c
        PARENT_SCOPE
        )


set(INCLUDE_DIRS
        ${INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        PARENT_SCOPE
        )
