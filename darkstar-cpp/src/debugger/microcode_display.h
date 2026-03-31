/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <wx/wx.h>
#include <wx/listctrl.h>

#include <cstdint>

namespace darkstar {

class CentralProcessor;

/// Presents a three-column virtual list view of microcode disassembly:
///
///   | Breakpoint | Address | Disassembly |
///
/// Breakpoint is a toggle column that sets/clears CP execution breakpoints.
class MicrocodeDisplay : public wxListCtrl {
public:
    MicrocodeDisplay(wxWindow* parent, wxWindowID id = wxID_ANY,
                     const wxPoint& pos = wxDefaultPosition,
                     const wxSize& size = wxDefaultSize);

    /// Returns the microcode address of the currently selected row, or -1.
    int selected_address() const;

    /// Attach the central processor whose microcode RAM is displayed.
    void attach_cp(CentralProcessor* cp);

    /// Scroll to and select the given address row.
    void select_address(int address);

    /// Force a full visual refresh of the list.
    void refresh_display();

private:
    // Virtual-list callback: supply text for a given row/column.
    wxString OnGetItemText(long row, long column) const override;

    void on_item_activated(wxListEvent& event);

    CentralProcessor* cp_;
    int row_count_;

    wxDECLARE_EVENT_TABLE();
};

} // namespace darkstar
