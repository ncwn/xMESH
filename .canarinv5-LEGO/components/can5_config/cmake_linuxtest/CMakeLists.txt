# For linux_host_test

set(SOURCE
        ${SOURCE}
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/can5_config_provider.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/can5_config_types.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/can5_config.c
        PARENT_SCOPE
        )


set(INCLUDE_DIRS
        ${INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        PARENT_SCOPE
        )
