set(srcdirs "src")
set(includedirs "include")
set(requires can5_common can5_hal can5_config can5_logger can5_storage can5_phy_io app_update esp_https_ota mbedtls)

if (CONFIG_CAN5_CMD_BUILD_TESTS)
   message(STATUS "Compiling Commander Tests")
   list(APPEND srcdirs "test")
   list(APPEND includedirs "test")
endif()

list(APPEND srcdirs "ccronexpr")
list(APPEND includedirs "ccronexpr")

# idf_component_register([[SRCS src1 src2 ...] | [[SRC_DIRS dir1 dir2 ...] [EXCLUDE_SRCS src1 src2 ...]]

idf_component_register(SRC_DIRS ${srcdirs}  #[EXCLUDE_SRCS src1 src2 ...]
                       INCLUDE_DIRS ${includedirs}
                    #    PRIV_INCLUDE_DIRS dir1 dir2
                       REQUIRES ${requires}
                    #    PRIV_REQUIRES component1 component2
                    #    LDFRAGMENTS ldfragment1 ldfragment2
                    #    REQUIRED_IDF_TARGETS target1 target2
                    #   EMBED_FILES
        #    EMBED_TXTFILES file1 file2
                    #    KCONFIG kconfig
                    #    KCONFIG_PROJBUILD kconfig_projbuild
                    )


target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=unused-variable -Werror=int-conversion -DCRON_USE_LOCAL_TIME)
