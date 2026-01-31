# For linux_host_test

set(SOURCE
        ${SOURCE}
        ${CMAKE_CURRENT_SOURCE_DIR}/../can5_storage_fsfat/lifo_storage_writer.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../can5_storage_fsfat/dict_storage_writer.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../can5_storage_fsfat/can5_storage_fsfat.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../can5_storage_ram/can5_storage_ram.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../can5_storage_nvs/can5_storage_nvs.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/can5_storagemng.c
        PARENT_SCOPE
)

set(INCLUDE_DIRS
        ${INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../can5_storage_fsfat
        ${CMAKE_CURRENT_SOURCE_DIR}/../can5_storage_ram
        ${CMAKE_CURRENT_SOURCE_DIR}/../can5_storage_nvs
        PARENT_SCOPE
        )