/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace darkstar {

struct SourceEntry {
    std::string source_path;
    std::vector<std::string> symbol_names;
    uint16_t address;
    int line_number;

    SourceEntry() : address(0), line_number(0) {}
    SourceEntry(const std::string& path, const std::vector<std::string>& symbols,
                uint16_t addr, int line)
        : source_path(path), symbol_names(symbols), address(addr), line_number(line) {}

    std::string to_string() const;
    static SourceEntry empty();
};

class SourceMap {
public:
    SourceMap(const std::string& map_name, const std::string& map_file, const std::string& source_root);

    const std::string& map_name() const { return map_name_; }
    const std::string& source_root() const { return source_root_; }

    std::vector<std::string> get_source_files() const;
    void save();

    const SourceEntry* get_source_for_address(uint16_t address) const;
    const SourceEntry* get_exact_source_for_address(uint16_t address) const;
    const SourceEntry* get_nearest_symbol_for_address(uint16_t address) const;
    bool get_address_for_source(const SourceEntry& entry, uint16_t& address) const;

    void add_source_entry(const SourceEntry& entry);
    void add_source_file(const std::string& source_path);
    void remove_source_entry(const SourceEntry& entry);

private:
    void read_map(const std::string& map_file, const std::string& source_root);
    void insert_address_entry(const SourceEntry& entry);
    void insert_source_entry(const SourceEntry& entry);

    std::map<std::string, std::vector<SourceEntry>> source_file_to_entry_map_;
    std::vector<SourceEntry> ordered_source_entries_;
    std::string map_name_;
    std::string map_file_;
    std::string source_root_;
};

} // namespace darkstar
