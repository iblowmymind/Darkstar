/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/

#include "debugger/source_display.h"
#include "debugger/breakpoint_manager.h"
#include "debugger/source_map.h"

#include <wx/filename.h>
#include <wx/msgdlg.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace darkstar {

wxBEGIN_EVENT_TABLE(SourceDisplay, wxListCtrl)
    EVT_LIST_ITEM_ACTIVATED(wxID_ANY, SourceDisplay::on_item_activated)
wxEND_EVENT_TABLE()

SourceDisplay::SourceDisplay(wxWindow* parent, wxWindowID id,
                             const wxPoint& pos, const wxSize& size)
    : wxListCtrl(parent, id, pos, size,
                 wxLC_REPORT | wxLC_VIRTUAL | wxLC_SINGLE_SEL),
      iop_code_(false),
      read_only_(true),
      source_map_(nullptr)
{
    AppendColumn("B",       wxLIST_FORMAT_CENTER, 30);
    AppendColumn("Address", wxLIST_FORMAT_LEFT,   70);
    AppendColumn("Source",  wxLIST_FORMAT_LEFT,   600);
}

int SourceDisplay::selected_address() const {
    if (!source_map_ || current_source_file_.empty()) return -1;

    long sel = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel < 0) return -1;

    SourceEntry query(current_source_file_, {}, 0, static_cast<int>(sel));
    uint16_t address = 0;
    if (source_map_->get_address_for_source(query, address)) {
        return static_cast<int>(address);
    }
    return -1;
}

void SourceDisplay::set_source_root(const std::string& source_root) {
    source_root_ = source_root;
}

void SourceDisplay::attach_map(SourceMap* source_map) {
    source_map_ = source_map;
}

void SourceDisplay::select_source_entry(const SourceEntry& entry,
                                        bool read_only, bool iop) {
    load_source_file(entry.source_path);
    select_line(entry.line_number);
    read_only_ = read_only;
    iop_code_ = iop;
}

void SourceDisplay::refresh_display() {
    long count = static_cast<long>(source_.size());
    if (count > 0) {
        RefreshItems(0, count - 1);
    }
}

wxString SourceDisplay::OnGetItemText(long row, long column) const {
    if (row < 0 || row >= static_cast<long>(source_.size())) {
        return wxEmptyString;
    }

    switch (column) {
        case 0: {
            // Breakpoint indicator -- only meaningful when an address mapping
            // exists for this source line.
            if (source_map_) {
                SourceEntry query(current_source_file_, {}, 0, static_cast<int>(row));
                uint16_t address = 0;
                if (source_map_->get_address_for_source(query, address)) {
                    auto proc = iop_code_ ? BreakpointProcessor::IOP
                                          : BreakpointProcessor::CP;
                    auto bp = BreakpointManager::instance().get_breakpoint(proc, address);
                    return (bp != BreakpointType::None) ? "*" : "";
                }
            }
            return "";
        }
        case 1: {
            // Address column -- show the mapped address in hex if available.
            if (source_map_) {
                SourceEntry query(current_source_file_, {}, 0, static_cast<int>(row));
                uint16_t address = 0;
                if (source_map_->get_address_for_source(query, address)) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "$%04x", static_cast<unsigned>(address));
                    return buf;
                }
            }
            return "";
        }
        case 2:
            return source_[static_cast<size_t>(row)];

        default:
            return wxEmptyString;
    }
}

void SourceDisplay::on_item_activated(wxListEvent& event) {
    long row = event.GetIndex();
    if (row < 0 || row >= static_cast<long>(source_.size())) return;
    if (!source_map_) return;

    SourceEntry query(current_source_file_, {}, 0, static_cast<int>(row));
    uint16_t address = 0;
    if (!source_map_->get_address_for_source(query, address)) return;

    auto proc = iop_code_ ? BreakpointProcessor::IOP : BreakpointProcessor::CP;
    auto& bpm = BreakpointManager::instance();
    auto current = bpm.get_breakpoint(proc, address);

    if (current != BreakpointType::None) {
        bpm.set_breakpoint(BreakpointEntry(proc, BreakpointType::None, address));
    } else {
        bpm.set_breakpoint(BreakpointEntry(proc, BreakpointType::Execution, address));
    }

    RefreshItem(row);
}

void SourceDisplay::load_source_file(const std::string& source_path) {
    if (current_source_file_ == source_path) return;

    try {
        source_.clear();

        std::string full_path = source_root_.empty()
            ? source_path
            : source_root_ + wxFileName::GetPathSeparator() + source_path;

        std::ifstream in(full_path);
        if (!in.is_open()) {
            source_.push_back("Unable to load source file " + source_path);
        } else {
            std::string line;
            while (std::getline(in, line)) {
                source_.push_back(untabify(line));
            }
        }

        SetItemCount(static_cast<long>(source_.size()));
        current_source_file_ = source_path;
        Refresh();
    } catch (const std::exception& e) {
        source_.clear();
        source_.push_back("Unable to load source file " + source_path
                          + ".  Error: " + e.what());
        SetItemCount(static_cast<long>(source_.size()));
        Refresh();
    }
}

void SourceDisplay::select_line(int line_number) {
    // Clear previous selection.
    long prev = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (prev >= 0) {
        SetItemState(prev, 0, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
    }

    if (line_number >= 0 && line_number < static_cast<int>(source_.size())) {
        SetItemState(line_number,
                     wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                     wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
        EnsureVisible(line_number);
    }
}

std::string SourceDisplay::untabify(const std::string& tabified) {
    std::string result;
    result.reserve(tabified.size());

    int column = 0;
    for (size_t i = 0; i < tabified.size(); ++i) {
        char c = tabified[i];
        if (c == '\t') {
            result += ' ';
            column++;
            while ((column % 4) != 0) {
                result += ' ';
                column++;
            }
        } else if (static_cast<unsigned char>(c) == 0xef &&
                   i + 2 < tabified.size() &&
                   static_cast<unsigned char>(tabified[i + 1]) == 0xbf &&
                   static_cast<unsigned char>(tabified[i + 2]) == 0xbd) {
            // UTF-8 encoding of U+FFFD (replacement character).
            // Substitute with a left-arrow character as in the original C#.
            result += "\xe2\x86\x90";  // UTF-8 for U+2190 LEFTWARDS ARROW
            i += 2;  // skip remaining 2 bytes of the 3-byte sequence
            column++;
        } else {
            result += c;
            column++;
        }
    }

    return result;
}

} // namespace darkstar
