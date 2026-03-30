/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#include "debugger/source_map.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace darkstar {

std::string SourceEntry::to_string() const {
    std::ostringstream oss;
    oss << source_path << ", line " << line_number << " address 0x"
        << std::hex << address;
    return oss.str();
}

SourceEntry SourceEntry::empty() {
    return SourceEntry("", {"*none*"}, 0, 0);
}

SourceMap::SourceMap(const std::string& map_name, const std::string& map_file, const std::string& source_root)
    : map_name_(map_name)
    , map_file_(map_file)
    , source_root_(source_root)
{
    read_map(map_file, source_root);
}

std::vector<std::string> SourceMap::get_source_files() const {
    std::vector<std::string> files;
    for (const auto& pair : source_file_to_entry_map_) {
        files.push_back(pair.first);
    }
    return files;
}

void SourceMap::save() {
    std::ofstream out(map_file_);
    if (!out.is_open()) return;

    out << "# \n"
        << "# This file maps assembly source symbols to PROM/microcode addresses and source lines.\n"
        << "#\n"
        << "# Each source file is given a header a la:\n"
        << "# [FooSource.asm]\n"
        << "#\n"
        << "# Each line's syntax is:\n"
        << "# <symbol name 1>, .. , <symbol name N>: <address (hex)>,<line number(decimal)>\n"
        << "#\n\n";

    for (const auto& pair : source_file_to_entry_map_) {
        out << "[" << pair.first << "]\n";
        for (const auto& entry : pair.second) {
            for (size_t i = 0; i < entry.symbol_names.size(); i++) {
                out << entry.symbol_names[i];
                if (i < entry.symbol_names.size() - 1) out << ",";
            }
            out << ": 0x" << std::hex << entry.address << "," << std::dec << (entry.line_number + 1) << "\n";
        }
        out << "\n";
    }
}

const SourceEntry* SourceMap::get_source_for_address(uint16_t address) const {
    const SourceEntry* result = nullptr;
    for (const auto& entry : ordered_source_entries_) {
        if (entry.address > address) break;
        result = &entry;
    }
    return result;
}

const SourceEntry* SourceMap::get_exact_source_for_address(uint16_t address) const {
    for (const auto& entry : ordered_source_entries_) {
        if (entry.address == address) return &entry;
    }
    return nullptr;
}

const SourceEntry* SourceMap::get_nearest_symbol_for_address(uint16_t address) const {
    const SourceEntry* result = nullptr;
    for (const auto& entry : ordered_source_entries_) {
        if (entry.address > address) break;
        if (!entry.symbol_names.empty() && entry.symbol_names[0] != "*none*") {
            result = &entry;
        }
    }
    return result;
}

bool SourceMap::get_address_for_source(const SourceEntry& entry, uint16_t& address) const {
    auto it = source_file_to_entry_map_.find(entry.source_path);
    if (it == source_file_to_entry_map_.end()) return false;

    for (const auto& line : it->second) {
        if (line.line_number == entry.line_number) {
            address = line.address;
            return true;
        }
    }
    return false;
}

void SourceMap::add_source_entry(const SourceEntry& entry) {
    insert_address_entry(entry);
    insert_source_entry(entry);
}

void SourceMap::add_source_file(const std::string& source_path) {
    if (source_file_to_entry_map_.find(source_path) != source_file_to_entry_map_.end()) {
        return;
    }
    source_file_to_entry_map_[source_path] = {};
}

void SourceMap::remove_source_entry(const SourceEntry& entry) {
    for (auto it = ordered_source_entries_.begin(); it != ordered_source_entries_.end(); ++it) {
        if (it->address == entry.address) {
            ordered_source_entries_.erase(it);
            break;
        }
    }

    auto map_it = source_file_to_entry_map_.find(entry.source_path);
    if (map_it != source_file_to_entry_map_.end()) {
        auto& vec = map_it->second;
        for (auto it = vec.begin(); it != vec.end(); ++it) {
            if (it->address == entry.address) {
                vec.erase(it);
                break;
            }
        }
    }
}

void SourceMap::insert_address_entry(const SourceEntry& entry) {
    // Remove any existing entry at this address
    for (auto it = ordered_source_entries_.begin(); it != ordered_source_entries_.end(); ++it) {
        if (it->address == entry.address) {
            ordered_source_entries_.erase(it);
            break;
        }
    }

    // Insert in sorted order
    auto pos = std::lower_bound(ordered_source_entries_.begin(), ordered_source_entries_.end(), entry,
        [](const SourceEntry& a, const SourceEntry& b) { return a.address < b.address; });
    ordered_source_entries_.insert(pos, entry);
}

void SourceMap::insert_source_entry(const SourceEntry& entry) {
    auto& vec = source_file_to_entry_map_[entry.source_path];
    // Remove existing entry for same line
    for (auto it = vec.begin(); it != vec.end(); ++it) {
        if (it->line_number == entry.line_number) {
            vec.erase(it);
            break;
        }
    }
    vec.push_back(entry);
}

void SourceMap::read_map(const std::string& map_file, const std::string& /*source_root*/) {
    std::ifstream in(map_file);
    if (!in.is_open()) return;

    std::string source_file;
    std::string line;

    while (std::getline(in, line)) {
        // Trim
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        // Skip comments
        if (line[0] == '#') continue;

        // File header
        if (line[0] == '[') {
            size_t end = line.find(']');
            if (end != std::string::npos) {
                source_file = line.substr(1, end - 1);
                add_source_file(source_file);
            }
            continue;
        }

        // Parse symbol entry: "sym1,sym2: 0xADDR,LINE"
        size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos) continue;

        std::string symbol_part = line.substr(0, colon_pos);
        std::string addr_part = line.substr(colon_pos + 1);

        // Parse symbols
        std::vector<std::string> symbols;
        std::istringstream sym_stream(symbol_part);
        std::string sym;
        while (std::getline(sym_stream, sym, ',')) {
            size_t s = sym.find_first_not_of(" \t");
            size_t e = sym.find_last_not_of(" \t");
            if (s != std::string::npos) {
                symbols.push_back(sym.substr(s, e - s + 1));
            }
        }

        // Parse address and line number
        size_t comma_pos = addr_part.find(',');
        if (comma_pos == std::string::npos) continue;

        std::string addr_str = addr_part.substr(0, comma_pos);
        std::string line_str = addr_part.substr(comma_pos + 1);

        // Trim
        size_t s = addr_str.find_first_not_of(" \t");
        if (s != std::string::npos) addr_str = addr_str.substr(s);

        uint16_t address = 0;
        if (addr_str.substr(0, 2) == "0x" || addr_str.substr(0, 2) == "0X") {
            address = static_cast<uint16_t>(std::stoul(addr_str, nullptr, 16));
        } else {
            address = static_cast<uint16_t>(std::stoul(addr_str));
        }

        int line_number = std::stoi(line_str) - 1; // Convert to 0-indexed

        SourceEntry entry(source_file, symbols, address, line_number);
        insert_address_entry(entry);
        insert_source_entry(entry);
    }
}

} // namespace darkstar
