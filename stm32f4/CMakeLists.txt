cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME sensor_demo)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    # --- App layer ---
    ${CMAKE_CURRENT_SOURCE_DIR}/app/queues.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app/task_sensor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app/task_send.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app/task_recv.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app/task_control.c
    # --- Services layer ---
    ${CMAKE_CURRENT_SOURCE_DIR}/services/protocol/frame_codec.c
    ${CMAKE_CURRENT_SOURCE_DIR}/services/uart_adapter/uart_hal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/services/sensors/manager.c
    ${CMAKE_CURRENT_SOURCE_DIR}/services/sensors/adapters.c
    ${CMAKE_CURRENT_SOURCE_DIR}/services/sensors/devices/DHT22.c
    ${CMAKE_CURRENT_SOURCE_DIR}/services/sensors/devices/DS18B20.c
    ${CMAKE_CURRENT_SOURCE_DIR}/services/sensors/devices/GY30.c
    ${CMAKE_CURRENT_SOURCE_DIR}/services/sensors/devices/SoilHumidity.c
    ${CMAKE_CURRENT_SOURCE_DIR}/services/sensors/devices/AHT30.c
    ${CMAKE_CURRENT_SOURCE_DIR}/services/pump/pump_driver.c
    ${CMAKE_CURRENT_SOURCE_DIR}/services/time/time_service.c
    
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    # --- App layer ---
    ${CMAKE_SOURCE_DIR}/app
    # --- Services layer ---
    ${CMAKE_SOURCE_DIR}/services/protocol
    ${CMAKE_SOURCE_DIR}/services/uart_adapter
    ${CMAKE_SOURCE_DIR}/services/sensors
    ${CMAKE_SOURCE_DIR}/services/sensors/devices
    ${CMAKE_SOURCE_DIR}/services/pump
    ${CMAKE_SOURCE_DIR}/services/time
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
