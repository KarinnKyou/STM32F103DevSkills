cmake_minimum_required(VERSION 3.20)

# Minimal CMake build for a Wildfire (野火小智) STM32F103C8T6 HAL example.
# Verified with: arm-none-eabi-gcc 14.3.1 + CMake 4.4 + Ninja 1.13

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(f103_station C ASM)   # 改成你的工程名

set(MCU_FLAGS "-mcpu=cortex-m3 -mthumb -mfloat-abi=soft")

set(CMAKE_C_FLAGS
    "${MCU_FLAGS} -std=gnu11 -Wall -ffunction-sections -fdata-sections -Og -g3")
set(CMAKE_ASM_FLAGS "${MCU_FLAGS} -x assembler-with-cpp")
set(CMAKE_EXE_LINKER_FLAGS
    "${MCU_FLAGS} -T${CMAKE_CURRENT_SOURCE_DIR}/linker/STM32F103C8Tx_FLASH.ld \
     -Wl,--gc-sections -Wl,--no-warn-rwx-segments -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map \
     --specs=nano.specs --specs=nosys.specs")

# nano.specs ships without float formatting: printf("%f") prints nothing.
# Uncomment to support it (costs roughly 6 KB of flash).
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -u _printf_float")

file(GLOB_RECURSE APP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)
file(GLOB_RECURSE HAL_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/drivers/STM32F1xx_HAL_Driver/Src/*.c)

# HAL ships *_template.c files (msp, timebase_rtc_alarm, timebase_tim) that are
# meant to be copied into a project, not compiled together: the two timebase
# templates both define HAL_InitTick/HAL_SuspendTick/HAL_ResumeTick, and linking
# them gives "multiple definition of HAL_InitTick".
list(FILTER HAL_SOURCES EXCLUDE REGEX "_template\\.c$")

add_executable(${PROJECT_NAME}
    startup/startup_stm32f103xb.s
    ${APP_SOURCES}
    ${HAL_SOURCES}
)

# Every directory under src/ and the two CMSIS/Drivers headers roots.
file(GLOB_RECURSE APP_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/*)
set(APP_INCLUDE_DIRS "")
file(GLOB_RECURSE _src_entries LIST_DIRECTORIES true ${CMAKE_CURRENT_SOURCE_DIR}/src/*)
foreach(_e ${_src_entries})
    if (IS_DIRECTORY ${_e})
        list(APPEND APP_INCLUDE_DIRS ${_e})
    endif()
endforeach()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${APP_INCLUDE_DIRS}
    src
    drivers/STM32F1xx_HAL_Driver/Inc
    drivers/CMSIS/Device/ST/STM32F1xx/Include
    drivers/CMSIS/Include
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    USE_HAL_DRIVER
    STM32F103xB
)

set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".elf")

# Optional demo build: the servo sweeps by itself so the wiring can be checked
# without touching the encoder.
#   cmake -S . -B build_sweep -DSERVO_DEMO_SWEEP=ON ...
option(SERVO_DEMO_SWEEP "let the servo sweep by itself (visual check)" OFF)
if (SERVO_DEMO_SWEEP)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SERVO_DEMO_SWEEP=1)
endif()

# Print size after every build
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}>
    COMMENT "firmware size"
)
