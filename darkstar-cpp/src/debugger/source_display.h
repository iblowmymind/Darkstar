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
#include <string>
#include <vector>

namespace darkstar {

class SourceMap;
struct SourceEntry;

/// Presents a source-code view aligned with symbol addresses and breakpoints.
/// One source file is loaded at a time.
///
/// Three columns:
///   | Breakpoint | Address | Source text |
///
/// Double-clicking a row toggles the execution breakpoint at that address
/// (when an address mapping exists).
class SourceDisplay : public wxListCtrl {
public:
    SourceDisplay(wxWindow* parent, wxWindowID id = wxID_ANY,
                  const wxPoint& pos = wxDefaultPosition,
                  const wxSize& size = wxDefaultSize);

    /// Path of the currently loaded source file (relative to source root).
    const std::string& current_source_file() const { return current_source_file_; }

    /// Returns the address mapped to the selected line, or -1.
    int selected_address() const;

    /// Access the attached source map.
    SourceMap* source_map() const { return source_map_; }

    /// Set the root directory under which source files live.
    void set_source_root(const std::string& source_root);

    /// Attach a SourceMap used for address/symbol lookups.
    void attach_map(SourceMap* source_map);

    /// Load the source file indicated by `entry`, highlight the line, and
    /// optionally mark the display as read-only / IOP code.
    void select_source_entry(const SourceEntry& entry, bool read_only, bool iop);

    /// Force a full visual refresh.
    void refresh_display();

private:
    wxString OnGetItemText(long row, long column) const override;

    void on_item_activated(wxListEvent& event);

    void load_source_file(const std::string& source_path);
    void select_line(int line_number);

    /// Convert tabs to 4-space tabulation and replace the unknown-Unicode
    /// placeholder with a left-arrow character.
    static std::string untabify(const std::string& tabified);

    std::string current_source_file_;
    std::string source_root_;
    std::vector<std::string> source_;
    bool iop_code_;
    bool read_only_;
    SourceMap* source_map_;

    static constexpr char ARROW_CHAR = '<';              // left-arrow replacement
    static constexpr char UNICODE_UNKNOWN = '\xef';      // first byte of UTF-8 U+FFFD

    wxDECLARE_EVENT_TABLE();
};

} // namespace darkstar
