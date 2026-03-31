/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <wx/wx.h>
#include <wx/listbox.h>

#include <cstdint>
#include <string>
#include <vector>

namespace darkstar {

class MicrocodeLoadMap;
class SourceMap;

/// Dialog for selecting or creating a microcode load to associate a source
/// file with.  Two groups: "Add to Existing Microcode Load" (list + Add/Cancel)
/// and "Add to New Microcode Load" (name/start/end fields + Create/Cancel).
class LoadMapDialog : public wxDialog {
public:
    LoadMapDialog(wxWindow* parent,
                  const std::string& new_file_path,
                  MicrocodeLoadMap* load_map,
                  std::vector<SourceMap*>& source_maps,
                  const uint64_t* microcode_ram,
                  int microcode_ram_size);

    /// Returns the map that was selected or newly created, or nullptr if
    /// the dialog was cancelled.
    SourceMap* selected_map() const { return selected_map_; }

private:
    void on_selection_changed(wxCommandEvent& event);
    void on_add_button(wxCommandEvent& event);
    void on_cancel_add_button(wxCommandEvent& event);
    void on_create_load_button(wxCommandEvent& event);
    void on_cancel_create_button(wxCommandEvent& event);

    std::string new_file_path_;
    MicrocodeLoadMap* load_map_;
    std::vector<SourceMap*>& source_maps_;
    const uint64_t* microcode_ram_;
    int microcode_ram_size_;

    SourceMap* selected_map_;

    // Controls -- existing load group
    wxListBox* current_loads_list_;
    wxButton* add_button_;
    wxButton* cancel_add_button_;

    // Controls -- new load group
    wxTextCtrl* load_name_text_;
    wxTextCtrl* load_start_box_;
    wxTextCtrl* load_end_box_;
    wxButton* create_load_button_;
    wxButton* cancel_create_button_;

    wxDECLARE_EVENT_TABLE();
};

} // namespace darkstar
