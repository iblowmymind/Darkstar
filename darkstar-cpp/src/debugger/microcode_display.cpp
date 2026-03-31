/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/

#include "debugger/microcode_display.h"
#include "debugger/breakpoint_manager.h"
#include "cp/central_processor.h"
#include "cp/microinstruction.h"

#include <cstdio>
#include <stdexcept>

namespace darkstar {

wxBEGIN_EVENT_TABLE(MicrocodeDisplay, wxListCtrl)
    EVT_LIST_ITEM_ACTIVATED(wxID_ANY, MicrocodeDisplay::on_item_activated)
wxEND_EVENT_TABLE()

MicrocodeDisplay::MicrocodeDisplay(wxWindow* parent, wxWindowID id,
                                   const wxPoint& pos, const wxSize& size)
    : wxListCtrl(parent, id, pos, size,
                 wxLC_REPORT | wxLC_VIRTUAL | wxLC_SINGLE_SEL),
      cp_(nullptr),
      row_count_(0)
{
    AppendColumn("B",           wxLIST_FORMAT_CENTER, 30);
    AppendColumn("Address",     wxLIST_FORMAT_LEFT,   70);
    AppendColumn("Disassembly", wxLIST_FORMAT_LEFT,   500);
}

int MicrocodeDisplay::selected_address() const {
    long sel = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    return (sel >= 0) ? static_cast<int>(sel) : -1;
}

void MicrocodeDisplay::attach_cp(CentralProcessor* cp) {
    cp_ = cp;
    // Microcode RAM is a fixed 4096-entry array.
    row_count_ = 4096;
    SetItemCount(row_count_);
    Refresh();
}

void MicrocodeDisplay::select_address(int address) {
    if (!cp_ || address < 0 || address >= row_count_) {
        throw std::invalid_argument("Invalid address.");
    }

    // Clear any previous selection.
    long prev = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (prev >= 0) {
        SetItemState(prev, 0, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
    }

    SetItemState(address, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                 wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
    EnsureVisible(address);
}

void MicrocodeDisplay::refresh_display() {
    if (row_count_ > 0) {
        RefreshItems(0, row_count_ - 1);
    }
}

wxString MicrocodeDisplay::OnGetItemText(long row, long column) const {
    if (!cp_ || row < 0 || row >= row_count_) {
        return wxEmptyString;
    }

    switch (column) {
        case 0: {
            // Breakpoint indicator.
            auto bp = BreakpointManager::instance().get_breakpoint(
                BreakpointProcessor::CP, static_cast<uint16_t>(row));
            return (bp != BreakpointType::None) ? "*" : "";
        }
        case 1: {
            // Hex address (3 digits).
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%03x", static_cast<unsigned>(row));
            return buf;
        }
        case 2: {
            // Disassemble the microinstruction.
            Microinstruction mi(cp_->microcode_ram()[row]);
            return mi.disassemble(-1);
        }
        default:
            return wxEmptyString;
    }
}

void MicrocodeDisplay::on_item_activated(wxListEvent& event) {
    long row = event.GetIndex();
    if (row < 0 || row >= row_count_) return;

    uint16_t address = static_cast<uint16_t>(row);
    auto& bpm = BreakpointManager::instance();
    auto current = bpm.get_breakpoint(BreakpointProcessor::CP, address);

    if (current != BreakpointType::None) {
        // Clear the breakpoint.
        bpm.set_breakpoint(BreakpointEntry(BreakpointProcessor::CP,
                                           BreakpointType::None, address));
    } else {
        // Set an execution breakpoint.
        bpm.set_breakpoint(BreakpointEntry(BreakpointProcessor::CP,
                                           BreakpointType::Execution, address));
    }

    RefreshItem(row);
}

} // namespace darkstar
