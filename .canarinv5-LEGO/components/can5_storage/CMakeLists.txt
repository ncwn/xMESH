set(srcdirs "src")
set(includedirs "include")

set(requires can5_common fatfs nvs_flash json)

if (CONFIG_CAN5_STORAGE_BUILD_TESTS)
   message(STATUS "Compiling Storage Tests")
   list(APPEND srcdirs "test")
   list(APPEND includedirs "test")
endif()

#list(APPEND srcdirs "can5_storage_sdspi")
#list(APPEND includedirs "can5_storage_sdspi")
list(APPEND srcdirs "can5_storage_nvs")
list(APPEND includedirs "can5_storage_nvs")
list(APPEND srcdirs "can5_storage_fsfat")
list(APPEND includedirs "can5_storage_fsfat")
list(APPEND srcdirs "can5_storage_ram")
list(APPEND includedirs "can5_storage_ram")

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


target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=unused-variable -Werror=int-conversion)
