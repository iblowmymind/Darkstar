/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <cstdint>
#include <memory>

namespace darkstar {

class Memory;
enum class TaskType;

class MemoryController {
public:
    MemoryController();
    ~MemoryController();

    void reset();

    uint16_t m_status() const { return m_status_; }
    int mar() const { return mar_; }
    uint16_t md() const { return md_; }

    // Expose memory for debugging.
    Memory& debug_memory() { return *mem_; }
    const Memory& debug_memory() const { return *mem_; }

    void set_m_ctl(uint16_t value);
    void load_mar(int address);
    void load_mdr(uint16_t value);
    uint16_t read_md(TaskType task, bool& valid);

private:
    int mar_ = 0;
    uint16_t md_ = 0;
    bool md_valid_ = true;
    uint16_t m_status_ = 0;

    std::unique_ptr<Memory> mem_;
};

} // namespace darkstar
