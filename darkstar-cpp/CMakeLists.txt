cmake_minimum_required(VERSION 3.16)
project(darkstar VERSION 1.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(ENABLE_ETHERNET "Enable Ethernet emulation (requires libpcap)" OFF)

# Find wxWidgets
find_package(wxWidgets REQUIRED COMPONENTS core base adv)
include(${wxWidgets_USE_FILE})

# Find SDL2 (audio only)
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(SDL2 REQUIRED sdl2)
else()
    find_package(SDL2 REQUIRED)
endif()

# Optional libpcap for Ethernet
if(ENABLE_ETHERNET)
    find_library(PCAP_LIBRARY NAMES pcap wpcap)
    find_path(PCAP_INCLUDE_DIR pcap.h)
    if(NOT PCAP_LIBRARY)
        message(FATAL_ERROR "ENABLE_ETHERNET is ON but libpcap was not found")
    endif()
    message(STATUS "Ethernet support enabled (libpcap: ${PCAP_LIBRARY})")
endif()

# Collect sources
set(CORE_SOURCES
    src/core/configuration.cpp
    src/core/conversion.cpp
    src/core/high_res_timer.cpp
    src/core/log.cpp
    src/core/scheduler.cpp
    src/core/system.cpp
)

set(CP_SOURCES
    src/cp/am2901.cpp
    src/cp/central_processor.cpp
    src/cp/central_processor_io.cpp
    src/cp/macro_instruction.cpp
    src/cp/microinstruction.cpp
)

set(IOP_SOURCES
    src/iop/beeper.cpp
    src/iop/dma_controller.cpp
    src/iop/floppy_controller.cpp
    src/iop/i8085.cpp
    src/iop/iop_io_bus.cpp
    src/iop/io_processor.cpp
    src/iop/iop_memory_bus.cpp
    src/iop/keyboard.cpp
    src/iop/misc_io.cpp
    src/iop/mouse.cpp
    src/iop/printer.cpp
    src/iop/rs232.cpp
    src/iop/tod_clock.cpp
)

set(IO_SOURCES
    src/io/floppy_disk.cpp
    src/io/floppy_drive.cpp
    src/io/sa1000.cpp
    src/io/shugart_controller.cpp
)

set(MEMORY_SOURCES
    src/memory/memory.cpp
    src/memory/memory_controller.cpp
)

set(DISPLAY_SOURCES
    src/display/display_controller.cpp
)

set(ETHERNET_SOURCES
    src/ethernet/crc32.cpp
    src/ethernet/ethernet_controller.cpp
    src/ethernet/host_ethernet.cpp
    src/ethernet/nethub_interface.cpp
)

set(DEBUGGER_SOURCES
    src/debugger/breakpoint_manager.cpp
    src/debugger/source_map.cpp
    src/debugger/microcode_load_map.cpp
    src/debugger/debugger_main.cpp
    src/debugger/cp_debugger.cpp
    src/debugger/iop_debugger.cpp
    src/debugger/load_map_dialog.cpp
    src/debugger/microcode_display.cpp
    src/debugger/source_display.cpp
)

set(UI_SOURCES
    src/ui/about_dialog.cpp
    src/ui/configuration_dialog.cpp
    src/ui/display_panel.cpp
    src/ui/main_window.cpp
)

# Combine all sources
set(ALL_SOURCES
    src/main.cpp
    ${CORE_SOURCES}
    ${CP_SOURCES}
    ${IOP_SOURCES}
    ${IO_SOURCES}
    ${MEMORY_SOURCES}
    ${DISPLAY_SOURCES}
    ${ETHERNET_SOURCES}
    ${DEBUGGER_SOURCES}
    ${UI_SOURCES}
)

add_executable(darkstar ${ALL_SOURCES})

target_include_directories(darkstar PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(darkstar PRIVATE
    ${wxWidgets_LIBRARIES}
    ${SDL2_LIBRARIES}
)

if(ENABLE_ETHERNET)
    target_compile_definitions(darkstar PRIVATE ENABLE_ETHERNET)
    target_include_directories(darkstar PRIVATE ${PCAP_INCLUDE_DIR})
    target_link_libraries(darkstar PRIVATE ${PCAP_LIBRARY})
endif()

# Platform-specific settings
if(WIN32)
    target_link_libraries(darkstar PRIVATE ws2_32)
    # Set Windows subsystem
    set_target_properties(darkstar PROPERTIES WIN32_EXECUTABLE TRUE)
elseif(APPLE)
    # macOS frameworks
    find_library(COCOA_FRAMEWORK Cocoa)
    target_link_libraries(darkstar PRIVATE ${COCOA_FRAMEWORK})
endif()

# Install
install(TARGETS darkstar RUNTIME DESTINATION bin)
install(FILES resources/darkstar.cfg DESTINATION share/darkstar)
