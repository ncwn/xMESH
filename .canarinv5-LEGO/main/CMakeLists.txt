set(includedirs include)
set(srcs main.c src/can5.c src/can5_init.c src/can5_ota_upgrade.c src/can5_tests.c)

idf_component_register(
        SRCS ${srcs}
        INCLUDE_DIRS ${includedirs}
        )

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=unused-variable -Werror=int-conversion)
