cmake_minimum_required(VERSION 3.21)

project(stm32f103rb-cpp LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/libopencm3.cmake)

set(TARGET_NAME stm32f103rb-app)
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/ld/stm32f103rb.ld)

add_executable(${TARGET_NAME}
    src/main.cpp
    src/startup_stm32f103rb.c
    src/syscalls.c
)

target_include_directories(${TARGET_NAME}
    PRIVATE
        ${LIBOPENCM3_DIR}/include
)

target_compile_definitions(${TARGET_NAME}
    PRIVATE
        STM32F1
)

target_compile_options(${TARGET_NAME}
    PRIVATE
        -mcpu=cortex-m3
        -mthumb
        -ffunction-sections
        -fdata-sections
        -fno-common
        -Wall
        -Wextra
        -Wpedantic
        $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
)

target_link_options(${TARGET_NAME}
    PRIVATE
        -mcpu=cortex-m3
        -mthumb
        -T${LINKER_SCRIPT}
        -Wl,--gc-sections
        -Wl,-Map=${TARGET_NAME}.map
        -Wl,--print-memory-usage
        --specs=nosys.specs
)

target_link_libraries(${TARGET_NAME}
    PRIVATE
        ${LIBOPENCM3_LIBRARY}
)

add_dependencies(${TARGET_NAME} libopencm3_build)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${TARGET_NAME}> ${TARGET_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${TARGET_NAME}> ${TARGET_NAME}.bin
    COMMENT "Generating ELF, HEX and BIN artifacts"
)

add_custom_target(flash
    COMMAND openocd -f board/st_nucleo_f103rb.cfg -c "program $<TARGET_FILE:${TARGET_NAME}> verify reset exit"
    DEPENDS ${TARGET_NAME}
    USES_TERMINAL
)
