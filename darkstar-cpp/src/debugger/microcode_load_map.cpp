/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#include "debugger/microcode_load_map.h"
#include "debugger/source_map.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <functional>
#include <sstream>

namespace darkstar {

MicrocodeLoadMap::MicrocodeLoadMap(const std::string& load_map_file, const std::string& source_root)
    : load_map_file_(load_map_file)
    , source_root_(source_root)
{
    read_load_map(load_map_file);
}

void MicrocodeLoadMap::save() {
    std::ofstream out(load_map_file_);
    if (!out.is_open()) return;

    out << "# Microcode load map\n"
        << "# Format: name,map_name,start,end,hash\n\n";

    for (const auto& entry : entries_) {
        out << entry.name << "," << entry.map_name << ","
            << std::hex << "0x" << entry.start << ",0x" << entry.end << ",";
        for (size_t i = 0; i < entry.hash.size(); i++) {
            out << std::hex << static_cast<int>(entry.hash[i]);
        }
        out << "\n";
    }
}

void MicrocodeLoadMap::add_entry(const LoadMapEntry& entry) {
    entries_.push_back(entry);
}

void MicrocodeLoadMap::remove_entry(const std::string& name) {
    entries_.erase(
        std::remove_if(entries_.begin(), entries_.end(),
            [&name](const LoadMapEntry& e) { return e.name == name; }),
        entries_.end());
}

SourceMap* MicrocodeLoadMap::find_map_for_address(int address) const {
    for (const auto& pair : loaded_maps_) {
        if (address >= pair.first.start && address <= pair.first.end) {
            return pair.second.get();
        }
    }
    return nullptr;
}

SourceMap* MicrocodeLoadMap::get_map(const std::string& map_name) const {
    for (const auto& pair : loaded_maps_) {
        if (pair.first.map_name == map_name) {
            return pair.second.get();
        }
    }
    return nullptr;
}

void MicrocodeLoadMap::load_maps(const uint64_t* microcode, int microcode_size) {
    loaded_maps_.clear();

    for (const auto& entry : entries_) {
        if (entry.start >= microcode_size || entry.end >= microcode_size) continue;

        auto hash = compute_hash(microcode, entry.start, entry.end);
        if (hash == entry.hash) {
            std::string map_file = source_root_ + "/" + entry.map_name;
            auto map = std::make_shared<SourceMap>(entry.name, map_file, source_root_);
            loaded_maps_.emplace_back(entry, map);
        }
    }
}

std::vector<uint8_t> MicrocodeLoadMap::compute_hash(const uint64_t* microcode, int start, int end) const {
    // Simple hash of the microcode range
    std::hash<uint64_t> hasher;
    size_t hash_val = 0;
    for (int i = start; i <= end; i++) {
        hash_val ^= hasher(microcode[i]) + 0x9e3779b9 + (hash_val << 6) + (hash_val >> 2);
    }

    std::vector<uint8_t> result(16, 0);
    std::memcpy(result.data(), &hash_val, std::min(sizeof(hash_val), result.size()));
    return result;
}

void MicrocodeLoadMap::read_load_map(const std::string& load_map_file) {
    std::ifstream in(load_map_file);
    if (!in.is_open()) return;

    std::string line;
    while (std::getline(in, line)) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        if (line[0] == '#') continue;

        std::istringstream iss(line);
        std::string name, map_name, start_str, end_str, hash_str;

        if (!std::getline(iss, name, ',')) continue;
        if (!std::getline(iss, map_name, ',')) continue;
        if (!std::getline(iss, start_str, ',')) continue;
        if (!std::getline(iss, end_str, ',')) continue;
        std::getline(iss, hash_str);

        int s = static_cast<int>(std::stoul(start_str, nullptr, 0));
        int e = static_cast<int>(std::stoul(end_str, nullptr, 0));

        std::vector<uint8_t> hash;
        for (size_t i = 0; i + 1 < hash_str.size(); i += 2) {
            hash.push_back(static_cast<uint8_t>(std::stoul(hash_str.substr(i, 2), nullptr, 16)));
        }

        entries_.emplace_back(name, map_name, s, e, hash);
    }
}

} // namespace darkstar
