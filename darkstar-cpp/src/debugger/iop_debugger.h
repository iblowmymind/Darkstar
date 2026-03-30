/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <wx/wx.h>
#include <wx/listctrl.h>

namespace darkstar {

class DSystem;

class IOPDebugger : public wxFrame {
public:
    IOPDebugger(wxWindow* parent, DSystem* system);

    void update_display();

private:
    void on_close(wxCloseEvent& event);
    void create_controls();
    void update_registers();
    void update_disassembly();

    DSystem* system_;

    wxStaticText* pc_label_;
    wxStaticText* sp_label_;
    wxStaticText* a_label_;
    wxStaticText* flags_label_;
    wxStaticText* bc_label_;
    wxStaticText* de_label_;
    wxStaticText* hl_label_;
    wxStaticText* halted_label_;
    wxListCtrl* disasm_list_;
    wxTextCtrl* memory_text_;

    wxDECLARE_EVENT_TABLE();
};

} // namespace darkstar
