# For linux_host_test

set(SOURCE
        ${SOURCE}
        ${CMAKE_CURRENT_SOURCE_DIR}/../ccronexpr/ccronexpr.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/can5_cron.c
        PARENT_SCOPE
        )


set(INCLUDE_DIRS
        ${INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../ccronexpr
        PARENT_SCOPE
        )
