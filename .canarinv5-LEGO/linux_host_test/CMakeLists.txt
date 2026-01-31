cmake_minimum_required(VERSION 3.10)

# set the project name
project(test-app)


# --------------------------------------#

# Essential Source files
set(SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/test_runner.c
        # add sources in current directory here
)

set(INCLUDE_DIRS '')
# --------------------------------------#

# Unity Framework
add_subdirectory(Unity)

# cJSON Framework
set(SOURCE
        ${SOURCE}
        ${CMAKE_CURRENT_SOURCE_DIR}/cJSON/cJSON.c
)

set(INCLUDE_DIRS
        ${INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/cJSON
)
# --------------------------------------#

# Mock
add_subdirectory(mock)
# --------------------------------------#

# Essential Includes
# --------------------------------------#

add_subdirectory(../components/can5_common/cmake_linuxtest can5_common)
add_subdirectory(../components/can5_hal/cmake_linuxtest can5_hal)
add_subdirectory(../components/can5_rtc/cmake_linuxtest can5_rtc)
add_subdirectory(../components/can5_phy_io/cmake_linuxtest can5_sensor)
add_subdirectory(../components/can5_cmdr/cmake_linuxtest can5_cmdr)

# Add can5 tests here


add_subdirectory(../components/can5_storage/cmake_linuxtest can5_storage)

add_subdirectory(../components/can5_config/cmake_linuxtest can5_config)

add_subdirectory(../components/can5_net/cmake_linuxtest can5_net)

add_subdirectory(../components/can5_httpserver/cmake_linuxtest can5_httpserver)

# --------------------------------------#

# Add Macro For Test Compilation
add_compile_definitions(CAN5_LINUX_HOST_TEST)
add_compile_definitions(CONFIG_CAN5_STORAGE_CRITICAL_ERRORS_MAX=5)
add_compile_definitions(CONFIG_LOG_DEFAULT_LEVEL=1)
add_compile_definitions(portMAX_DELAY=1)
add_compile_definitions(CONFIG_CAN5_STORAGE_LOG_NUM_MAX=9)
add_compile_definitions(CONFIG_CAN5_STORAGE_LOG_SIZE_MAX=4)
add_compile_definitions(CONFIG_CAN5_KEEP_CACHE_FILE_NUM=4)
add_compile_definitions(configMAX_PRIORITIES=20)
# Other options
add_compile_options(-Wno-format)
include_directories(${INCLUDE_DIRS})


add_executable(test-app ${SOURCE})
target_link_libraries(test-app unity)
# Copy pretest script to build
add_custom_command(TARGET test-app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/pre_test.sh
        ${CMAKE_CURRENT_BINARY_DIR}/pre_test.sh)
