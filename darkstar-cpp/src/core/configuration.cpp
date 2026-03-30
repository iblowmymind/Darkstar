/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/
#include "core/configuration.h"
#include "core/log.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

namespace darkstar {

// StartupOptions statics
std::string StartupOptions::configuration_file;
std::string StartupOptions::rom_path;
bool StartupOptions::start = false;

// Configuration statics - defaults
uint32_t Configuration::memory_size = 768;
std::string Configuration::hard_drive_image;
std::string Configuration::floppy_drive_image;
uint64_t Configuration::host_id = 0x0000aa012345ULL;
std::string Configuration::host_packet_interface_name;
std::string Configuration::nethub_host = "localhost";
uint16_t Configuration::nethub_port = 3333;
bool Configuration::host_raw_ethernet_interfaces_available = false;
bool Configuration::throttle_speed = true;
uint32_t Configuration::display_scale = 1;
bool Configuration::slow_phosphor = true;
bool Configuration::full_screen_stretch = false;
TODPowerUpSetMode Configuration::tod_set_mode = TODPowerUpSetMode::HostTimeY2K;
int64_t Configuration::tod_date_time = 0;
int64_t Configuration::tod_date = 0;
LogComponent Configuration::log_components = LogComponent::None;
LogType Configuration::log_types = LogType::None;
AltBootValues Configuration::alt_boot_mode = AltBootValues::None;
bool Configuration::start = false;

static std::string to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

void Configuration::set_field(const std::string& name, const std::string& value) {
    std::string lower_name = to_lower(name);

    if (lower_name == "memorysize") {
        memory_size = static_cast<uint32_t>(std::stoul(value));
    } else if (lower_name == "harddriveimage") {
        hard_drive_image = value;
    } else if (lower_name == "floppydriveimage") {
        floppy_drive_image = value;
    } else if (lower_name == "hostid") {
        host_id = std::stoull(value, nullptr, 16);
    } else if (lower_name == "hostpacketinterfacename") {
        host_packet_interface_name = value;
    } else if (lower_name == "nethubhost") {
        nethub_host = value;
    } else if (lower_name == "nethubport") {
        nethub_port = static_cast<uint16_t>(std::stoul(value));
    } else if (lower_name == "throttlespeed") {
        throttle_speed = (to_lower(value) == "true");
    } else if (lower_name == "displayscale") {
        display_scale = static_cast<uint32_t>(std::stoul(value));
    } else if (lower_name == "slowphosphor") {
        slow_phosphor = (to_lower(value) == "true");
    } else if (lower_name == "fullscreenstretch") {
        full_screen_stretch = (to_lower(value) == "true");
    } else if (lower_name == "todsetmode") {
        std::string lv = to_lower(value);
        if (lv == "hosttimey2k") tod_set_mode = TODPowerUpSetMode::HostTimeY2K;
        else if (lv == "hosttime") tod_set_mode = TODPowerUpSetMode::HostTime;
        else if (lv == "specificdateandtime") tod_set_mode = TODPowerUpSetMode::SpecificDateAndTime;
        else if (lv == "specificdate") tod_set_mode = TODPowerUpSetMode::SpecificDate;
        else if (lv == "nochange") tod_set_mode = TODPowerUpSetMode::NoChange;
    } else if (lower_name == "altbootmode") {
        std::string lv = to_lower(value);
        if (lv == "none") alt_boot_mode = AltBootValues::None;
        else if (lv == "diagnosticrigid") alt_boot_mode = AltBootValues::DiagnosticRigid;
        else if (lv == "rigid") alt_boot_mode = AltBootValues::Rigid;
        else if (lv == "floppy") alt_boot_mode = AltBootValues::Floppy;
        else if (lv == "ethernet") alt_boot_mode = AltBootValues::Ethernet;
        else if (lv == "diagnosticethernet") alt_boot_mode = AltBootValues::DiagnosticEthernet;
        else if (lv == "diagnosticfloppy") alt_boot_mode = AltBootValues::DiagnosticFloppy;
        else if (lv == "alternateethernet") alt_boot_mode = AltBootValues::AlternateEthernet;
    } else if (lower_name == "start") {
        Configuration::start = (to_lower(value) == "true");
    } else {
        std::cerr << "Unknown configuration parameter: " << name << std::endl;
    }
}

void Configuration::read_configuration_file() {
    std::string config_path;

    if (!StartupOptions::configuration_file.empty()) {
        config_path = StartupOptions::configuration_file;
    } else {
        config_path = "Darkstar.cfg";
    }

    std::ifstream file(config_path);
    if (!file.is_open()) {
        std::cerr << "Configuration file " << config_path
                  << " does not exist or cannot be accessed. Using default settings."
                  << std::endl;
        return;
    }

    int line_number = 0;
    std::string line;
    while (std::getline(file, line)) {
        line_number++;
        line = trim(line);

        if (line.empty() || line[0] == '#') {
            continue;
        }

        auto eq_pos = line.find('=');
        if (eq_pos == std::string::npos) {
            std::cerr << config_path << " line " << line_number
                      << ": Invalid syntax." << std::endl;
            continue;
        }

        std::string parameter = trim(line.substr(0, eq_pos));
        std::string value = trim(line.substr(eq_pos + 1));

        try {
            set_field(parameter, value);
        } catch (const std::exception& e) {
            std::cerr << config_path << " line " << line_number
                      << ": Value '" << value << "' is invalid for parameter '"
                      << parameter << "'." << std::endl;
        }
    }
}

void Configuration::read_configuration() {
    try {
        read_configuration_file();
    } catch (...) {
        Log::write(LogType::Error, LogComponent::Configuration,
                   "Unable to load configuration. Assuming default settings.");
    }

    // Sanity-check settings
    if (memory_size > 768 || memory_size == 0 || (memory_size % 128) != 0) {
        Log::write(LogType::Error, LogComponent::Configuration,
                   "MemorySize configuration parameter is incorrect, defaulting to 768KW.");
        memory_size = 768;
    }

    if (display_scale < 1) {
        Log::write(LogType::Error, LogComponent::Configuration,
                   "DisplayScale configuration parameter is incorrect, defaulting to 1.");
        display_scale = 1;
    }

    start |= StartupOptions::start;
}

void Configuration::write_configuration() {
    // Write configuration to file
    std::string config_path;
    if (!StartupOptions::configuration_file.empty()) {
        config_path = StartupOptions::configuration_file;
    } else {
        config_path = "Darkstar.cfg";
    }

    std::ofstream file(config_path);
    if (!file.is_open()) {
        std::cerr << "Unable to write configuration file: " << config_path << std::endl;
        return;
    }

    file << "# Darkstar Configuration\n";
    file << "MemorySize=" << memory_size << "\n";
    if (!hard_drive_image.empty())
        file << "HardDriveImage=" << hard_drive_image << "\n";
    if (!floppy_drive_image.empty())
        file << "FloppyDriveImage=" << floppy_drive_image << "\n";

    char hex_buf[32];
    std::snprintf(hex_buf, sizeof(hex_buf), "%012llx",
                  static_cast<unsigned long long>(host_id));
    file << "HostID=" << hex_buf << "\n";

    if (!host_packet_interface_name.empty())
        file << "HostPacketInterfaceName=" << host_packet_interface_name << "\n";
    file << "NetHubHost=" << nethub_host << "\n";
    file << "NetHubPort=" << nethub_port << "\n";
    file << "ThrottleSpeed=" << (throttle_speed ? "True" : "False") << "\n";
    file << "DisplayScale=" << display_scale << "\n";
    file << "SlowPhosphor=" << (slow_phosphor ? "True" : "False") << "\n";
    file << "FullScreenStretch=" << (full_screen_stretch ? "True" : "False") << "\n";
}

} // namespace darkstar
