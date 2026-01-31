# For linux test app

set(SOURCE
        ${SOURCE}
        ${CMAKE_CURRENT_SOURCE_DIR}/../can5_protocols/can5_hazemon/can5_hazemon_types.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../can5_protocols/can5_hazemon/can5_codec_hazemon.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../can5_protocols/can5_lorarelay/can5_codec_lwan.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../can5_protocols/can5_lorarelay/can5_loramsg.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../can5_protocols/can5_mqtt/can5_codec_mqtt.c
        PARENT_SCOPE
)

set(INCLUDE_DIRS
        ${INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/../can5_protocols
        ${CMAKE_CURRENT_SOURCE_DIR}/../can5_protocols/can5_hazemon
        ${CMAKE_CURRENT_SOURCE_DIR}/../can5_protocols/can5_lorarelay
        ${CMAKE_CURRENT_SOURCE_DIR}/../can5_protocols/can5_mqtt
        ${CMAKE_CURRENT_SOURCE_DIR}/../can5_netif/can5_netif_lwan
        ${CMAKE_CURRENT_SOURCE_DIR}/../can5_netif/can5_netif_wifi
        ${CMAKE_CURRENT_SOURCE_DIR}/../can5_netif/can5_cell
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        PARENT_SCOPE
        )
