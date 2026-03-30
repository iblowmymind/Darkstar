/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace darkstar {

class SourceMap;

struct LoadMapEntry {
    std::string name;
    std::string map_name;
    int start;
    int end;
    std::vector<uint8_t> hash;

    LoadMapEntry() : start(0), end(0) {}
    LoadMapEntry(const std::string& n, const std::string& mn, int s, int e, const std::vector<uint8_t>& h)
        : name(n), map_name(mn), start(s), end(e), hash(h) {}
};

class MicrocodeLoadMap {
public:
    MicrocodeLoadMap(const std::string& load_map_file, const std::string& source_root);

    void save();
    void add_entry(const LoadMapEntry& entry);
    void remove_entry(const std::string& name);

    const std::vector<LoadMapEntry>& entries() const { return entries_; }

    SourceMap* find_map_for_address(int address) const;
    SourceMap* get_map(const std::string& map_name) const;

    void load_maps(const uint64_t* microcode, int microcode_size);

private:
    void read_load_map(const std::string& load_map_file);
    std::vector<uint8_t> compute_hash(const uint64_t* microcode, int start, int end) const;

    std::string load_map_file_;
    std::string source_root_;
    std::vector<LoadMapEntry> entries_;
    std::vector<std::pair<LoadMapEntry, std::shared_ptr<SourceMap>>> loaded_maps_;
};

} // namespace darkstar
