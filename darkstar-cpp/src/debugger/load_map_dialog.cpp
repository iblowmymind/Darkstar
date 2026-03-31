/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/

#include "debugger/load_map_dialog.h"
#include "debugger/microcode_load_map.h"
#include "debugger/source_map.h"

#include <wx/msgdlg.h>
#include <wx/filename.h>

#include <cstdlib>
#include <sstream>
#include <stdexcept>

namespace darkstar {

enum {
    ID_CURRENT_LOADS_LIST = wxID_HIGHEST + 1,
    ID_ADD_BUTTON,
    ID_CANCEL_ADD_BUTTON,
    ID_CREATE_LOAD_BUTTON,
    ID_CANCEL_CREATE_BUTTON,
};

wxBEGIN_EVENT_TABLE(LoadMapDialog, wxDialog)
    EVT_LISTBOX(ID_CURRENT_LOADS_LIST, LoadMapDialog::on_selection_changed)
    EVT_BUTTON(ID_ADD_BUTTON, LoadMapDialog::on_add_button)
    EVT_BUTTON(ID_CANCEL_ADD_BUTTON, LoadMapDialog::on_cancel_add_button)
    EVT_BUTTON(ID_CREATE_LOAD_BUTTON, LoadMapDialog::on_create_load_button)
    EVT_BUTTON(ID_CANCEL_CREATE_BUTTON, LoadMapDialog::on_cancel_create_button)
wxEND_EVENT_TABLE()

LoadMapDialog::LoadMapDialog(wxWindow* parent,
                             const std::string& new_file_path,
                             MicrocodeLoadMap* load_map,
                             std::vector<SourceMap*>& source_maps,
                             const uint64_t* microcode_ram,
                             int microcode_ram_size)
    : wxDialog(parent, wxID_ANY, "Select Microcode Load For File",
               wxDefaultPosition, wxSize(380, 180),
               wxDEFAULT_DIALOG_STYLE),
      new_file_path_(new_file_path),
      load_map_(load_map),
      source_maps_(source_maps),
      microcode_ram_(microcode_ram),
      microcode_ram_size_(microcode_ram_size),
      selected_map_(nullptr)
{
    auto* main_sizer = new wxBoxSizer(wxHORIZONTAL);

    // --- Left group: Add to Existing Microcode Load ---
    auto* existing_box = new wxStaticBoxSizer(wxVERTICAL, this,
                                              "Add to Existing Microcode Load");

    current_loads_list_ = new wxListBox(existing_box->GetStaticBox(),
                                        ID_CURRENT_LOADS_LIST,
                                        wxDefaultPosition, wxSize(160, 82));

    for (auto* sm : source_maps_) {
        current_loads_list_->Append(sm->map_name());
    }
    current_loads_list_->SetSelection(wxNOT_FOUND);

    existing_box->Add(current_loads_list_, 1, wxEXPAND | wxALL, 4);

    auto* existing_btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    add_button_ = new wxButton(existing_box->GetStaticBox(), ID_ADD_BUTTON,
                               "Add", wxDefaultPosition, wxSize(75, -1));
    cancel_add_button_ = new wxButton(existing_box->GetStaticBox(),
                                      ID_CANCEL_ADD_BUTTON, "Cancel",
                                      wxDefaultPosition, wxSize(75, -1));
    add_button_->Enable(false);

    existing_btn_sizer->Add(add_button_, 0, wxRIGHT, 4);
    existing_btn_sizer->Add(cancel_add_button_, 0);
    existing_box->Add(existing_btn_sizer, 0, wxALL, 4);

    main_sizer->Add(existing_box, 0, wxALL | wxEXPAND, 4);

    // --- Right group: Add to New Microcode Load ---
    auto* new_box = new wxStaticBoxSizer(wxVERTICAL, this,
                                         "Add to New Microcode Load");
    auto* grid = new wxFlexGridSizer(3, 2, 4, 4);

    grid->Add(new wxStaticText(new_box->GetStaticBox(), wxID_ANY, "Name:"),
              0, wxALIGN_CENTER_VERTICAL);
    load_name_text_ = new wxTextCtrl(new_box->GetStaticBox(), wxID_ANY, "",
                                     wxDefaultPosition, wxSize(100, -1));
    grid->Add(load_name_text_, 1, wxEXPAND);

    grid->Add(new wxStaticText(new_box->GetStaticBox(), wxID_ANY, "Start:"),
              0, wxALIGN_CENTER_VERTICAL);
    load_start_box_ = new wxTextCtrl(new_box->GetStaticBox(), wxID_ANY, "",
                                     wxDefaultPosition, wxSize(100, -1));
    grid->Add(load_start_box_, 1, wxEXPAND);

    grid->Add(new wxStaticText(new_box->GetStaticBox(), wxID_ANY, "End:"),
              0, wxALIGN_CENTER_VERTICAL);
    load_end_box_ = new wxTextCtrl(new_box->GetStaticBox(), wxID_ANY, "",
                                   wxDefaultPosition, wxSize(100, -1));
    grid->Add(load_end_box_, 1, wxEXPAND);

    new_box->Add(grid, 0, wxALL, 4);

    auto* new_btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    create_load_button_ = new wxButton(new_box->GetStaticBox(),
                                       ID_CREATE_LOAD_BUTTON, "Create",
                                       wxDefaultPosition, wxSize(75, -1));
    cancel_create_button_ = new wxButton(new_box->GetStaticBox(),
                                         ID_CANCEL_CREATE_BUTTON, "Cancel",
                                         wxDefaultPosition, wxSize(75, -1));

    new_btn_sizer->Add(create_load_button_, 0, wxRIGHT, 4);
    new_btn_sizer->Add(cancel_create_button_, 0);
    new_box->Add(new_btn_sizer, 0, wxALL, 4);

    main_sizer->Add(new_box, 0, wxALL | wxEXPAND, 4);

    SetSizerAndFit(main_sizer);
}

void LoadMapDialog::on_selection_changed(wxCommandEvent& /*event*/) {
    add_button_->Enable(current_loads_list_->GetSelection() != wxNOT_FOUND);
}

void LoadMapDialog::on_add_button(wxCommandEvent& /*event*/) {
    int sel = current_loads_list_->GetSelection();
    if (sel == wxNOT_FOUND) return;

    selected_map_ = source_maps_[sel];
    selected_map_->add_source_file(new_file_path_);

    EndModal(wxID_OK);
}

void LoadMapDialog::on_cancel_add_button(wxCommandEvent& /*event*/) {
    EndModal(wxID_CANCEL);
}

void LoadMapDialog::on_create_load_button(wxCommandEvent& /*event*/) {
    try {
        std::string name = load_name_text_->GetValue().ToStdString();
        std::string start_str = load_start_box_->GetValue().ToStdString();
        std::string end_str = load_end_box_->GetValue().ToStdString();

        // Trim whitespace
        auto trim = [](std::string& s) {
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
        };
        trim(start_str);
        trim(end_str);

        int start = static_cast<int>(std::strtol(start_str.c_str(), nullptr, 16));
        int end   = static_cast<int>(std::strtol(end_str.c_str(), nullptr, 16));

        if (end <= start || start < 0 || end > microcode_ram_size_) {
            wxMessageBox("Invalid start/end parameters.", "Error", wxOK | wxICON_ERROR, this);
            return;
        }

        // Build the entry with a hash computed over the microcode range.
        // We reuse MicrocodeLoadMap::add_entry which stores it.
        LoadMapEntry new_entry;
        new_entry.name = name;
        // map_name is derived from the load name (same convention as C# side)
        new_entry.map_name = name;
        new_entry.start = start;
        new_entry.end = end;

        load_map_->add_entry(new_entry);

        // Build source path components following C# convention:
        //   Path.Combine("CP", "Source", entry.MapName)
        std::string map_file = std::string("CP") + wxFileName::GetPathSeparator()
                             + "Source" + wxFileName::GetPathSeparator()
                             + new_entry.map_name;
        std::string source_root = std::string("CP") + wxFileName::GetPathSeparator()
                                + "Source";

        auto* new_map = new SourceMap(new_entry.name, map_file, source_root);
        source_maps_.push_back(new_map);

        selected_map_ = new_map;

        EndModal(wxID_OK);
    } catch (const std::exception& ex) {
        wxMessageBox(wxString::Format("Error: %s", ex.what()),
                     "Error", wxOK | wxICON_ERROR, this);
    }
}

void LoadMapDialog::on_cancel_create_button(wxCommandEvent& /*event*/) {
    EndModal(wxID_CANCEL);
}

} // namespace darkstar
