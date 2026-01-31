# For linux_host_test

set(SOURCE
        ${SOURCE}
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/can5_http_json.c
        PARENT_SCOPE
        )

set(INCLUDE_DIRS
        ${INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        PARENT_SCOPE
        )
