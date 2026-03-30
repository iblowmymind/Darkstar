/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <wx/wx.h>
#include <wx/textctrl.h>
#include <string>
#include <memory>

namespace darkstar {

class DSystem;
class CPDebugger;
class IOPDebugger;

enum class DebuggerReason {
    UserInvoked,
    Error,
};

class DebuggerMain : public wxFrame {
public:
    DebuggerMain(wxWindow* parent, DSystem* system, DebuggerReason reason, const std::string& message);
    ~DebuggerMain();

    void write_line(const std::string& text);

private:
    void on_close(wxCloseEvent& event);
    void on_key_down(wxKeyEvent& event);
    void on_command_enter(wxCommandEvent& event);

    void process_command(const std::string& command);
    void stop_execution();
    void start_execution();
    void display_current_code();
    void print_iop_status();
    void print_cp_status();
    void print_mesa_status();

    DSystem* system_;
    DebuggerReason reason_;
    std::string entry_message_;

    wxTextCtrl* output_text_;
    wxTextCtrl* command_input_;

    CPDebugger* cp_debugger_;
    IOPDebugger* iop_debugger_;

    wxDECLARE_EVENT_TABLE();
};

} // namespace darkstar
