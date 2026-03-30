/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <cstdint>
#include <string>
#include "core/log.h"

namespace darkstar {

enum class TODPowerUpSetMode {
    HostTimeY2K = 0,
    HostTime,
    SpecificDateAndTime,
    SpecificDate,
    NoChange
};

enum class AltBootValues : int {
    None = -1,
    DiagnosticRigid = 0,
    Rigid = 1,
    Floppy = 2,
    Ethernet = 3,
    DiagnosticEthernet = 4,
    DiagnosticFloppy = 5,
    AlternateEthernet = 6,
    DiagnosticTrident1 = 7,
    DiagnosticTrident2 = 8,
    DiagnosticTrident3 = 9,
    HeadCleaning = 10
};

struct StartupOptions {
    static std::string configuration_file;
    static std::string rom_path;
    static bool start;
};

class Configuration {
public:
    static void read_configuration();
    static void write_configuration();

    // System
    static uint32_t memory_size;
    static std::string hard_drive_image;
    static std::string floppy_drive_image;
    static uint64_t host_id;
    static std::string host_packet_interface_name;
    static std::string nethub_host;
    static uint16_t nethub_port;
    static bool host_raw_ethernet_interfaces_available;
    static bool throttle_speed;

    // Display
    static uint32_t display_scale;
    static bool slow_phosphor;
    static bool full_screen_stretch;

    // TOD Clock
    static TODPowerUpSetMode tod_set_mode;
    // Stored as seconds since Unix epoch for simplicity
    static int64_t tod_date_time;
    static int64_t tod_date;

    // Logging
    static LogComponent log_components;
    static LogType log_types;

    // Boot
    static AltBootValues alt_boot_mode;
    static bool start;

private:
    static void read_configuration_file();
    static void set_field(const std::string& name, const std::string& value);
};

} // namespace darkstar
