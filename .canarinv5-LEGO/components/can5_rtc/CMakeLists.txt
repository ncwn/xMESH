set(srcdirs "src")
set(includedirs "include")

if (CONFIG_CAN5_RTC_BUILD_TESTS)
    message(STATUS "Compiling RTC Tests")
    list(APPEND srcdirs "test")
    list(APPEND includedirs "test")
else()
    message(STATUS "No RTC Tests")
endif()

set(requires "can5_common" "can5_hal")

set(COMPONENT_PRIV_REQUIRES)

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