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

class CPDebugger : public wxFrame {
public:
    CPDebugger(wxWindow* parent, DSystem* system);

    void update_display();

private:
    void on_close(wxCloseEvent& event);
    void create_controls();
    void update_registers();
    void update_microcode_view();

    DSystem* system_;

    wxStaticText* task_label_;
    wxStaticText* tpc_label_;
    wxStaticText* cycle_label_;
    wxStaticText* alu_label_;
    wxStaticText* stackp_label_;
    wxStaticText* ib_label_;
    wxListCtrl* microcode_list_;
    wxTextCtrl* u_regs_text_;
    wxTextCtrl* rh_regs_text_;

    wxDECLARE_EVENT_TABLE();
};

} // namespace darkstar
