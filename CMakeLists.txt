cmake_minimum_required(VERSION 3.16)
project(CardioStream_IoT_Framework LANGUAGES CXX)

# ============================================================================
# Project Configuration
# ============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Detect if this is ESP32 (Arduino-based) environment
if(DEFINED ARDUINO)
    message(STATUS "Building for Arduino/ESP32 platform")
    set(PLATFORM_TYPE "ARDUINO")
else()
    message(STATUS "Building for desktop (simulation/testing)")
    set(PLATFORM_TYPE "DESKTOP")
endif()

# ============================================================================
# Source Files
# ============================================================================

set(CORE_HEADERS
    src/IBiomedicalSensor.h
    src/SignalBuffer.h
    src/WindowExtractor.h
    src/BiomedicalPipeline.h
)

# ============================================================================
# Build: Example Application
# ============================================================================

# Main example: ECG + AD8232 Mock Sensor
add_executable(cardiostream_ecg_demo
    examples/ecg_brazalete/main.cpp
)

# Include directories
target_include_directories(cardiostream_ecg_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# For Arduino/ESP32 builds, link Arduino libraries
if(PLATFORM_TYPE STREQUAL "ARDUINO")
    # Arduino libraries would be linked here
    message(STATUS "Arduino platform detected - ensure Arduino.h is available")
else()
    # For desktop testing, we might mock Arduino.h
    message(STATUS "Desktop platform - mock Arduino.h may be required")
endif()

# ============================================================================
# Compiler Flags
# ============================================================================

if(MSVC)
    target_compile_options(cardiostream_ecg_demo PRIVATE /W4 /WX)
else()
    target_compile_options(cardiostream_ecg_demo PRIVATE 
        -Wall -Wextra -Wpedantic -Wno-unused
    )
endif()

# ============================================================================
# Summary
# ============================================================================

message(STATUS "")
message(STATUS "CardioStream IoT Framework - Build Summary")
message(STATUS "  Platform: ${PLATFORM_TYPE}")
message(STATUS "  Example: cardiostream_ecg_demo")
message(STATUS "  Headers: ${CORE_HEADERS}")
message(STATUS "")
