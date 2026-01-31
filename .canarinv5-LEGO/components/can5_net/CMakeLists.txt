set(srcdirs "src")
set(includedirs "include")

set(requires can5_common can5_config can5_rtc can5_cmdr can5_mqtt_client)

if (CONFIG_CAN5_NET_BUILD_TESTS)
   message(STATUS "Compiling NET Tests")
   list(APPEND srcdirs "test")
   list(APPEND includedirs "test")
endif()

if (CONFIG_CAN5_NET_SYNC_HAZEMON_SERVER)
   message(STATUS "Sync with Hazemon Server")
endif()


# NIC drivers
# list(APPEND srcdirs "can5_netif_template")
list(APPEND srcdirs "can5_netif/can5_netif_lwan")
list(APPEND includedirs "can5_netif/can5_netif_lwan")
list(APPEND srcdirs "can5_netif/can5_netif_wrapper")
list(APPEND includedirs "can5_netif/can5_netif_wrapper")

# Data codec - packet handlers
list(APPEND includedirs "can5_protocols/can5_hazemon")
list(APPEND srcdirs "can5_protocols/can5_hazemon")
list(APPEND includedirs "can5_protocols/can5_lorarelay")
list(APPEND srcdirs "can5_protocols/can5_lorarelay")
list(APPEND includedirs "can5_protocols/can5_mqtt")
list(APPEND srcdirs "can5_protocols/can5_mqtt")
# idf_component_register([[SRCS src1 src2 ...] | [[SRC_DIRS dir1 dir2 ...] [EXCLUDE_SRCS src1 src2 ...]]

idf_component_register(SRC_DIRS ${srcdirs}  #[EXCLUDE_SRCS src1 src2 ...]
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


target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=unused-variable -Werror=int-conversion -Wno-error=format-truncation -Wno-error=stringop-truncation)
