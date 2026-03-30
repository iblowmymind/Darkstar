/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#include "debugger/debugger_main.h"
#include "debugger/cp_debugger.h"
#include "debugger/iop_debugger.h"
#include "core/system.h"
#include "cp/central_processor.h"
#include "iop/io_processor.h"

#include <sstream>
#include <iomanip>

namespace darkstar {

enum {
    ID_CommandInput = wxID_HIGHEST + 1,
};

wxBEGIN_EVENT_TABLE(DebuggerMain, wxFrame)
    EVT_CLOSE(DebuggerMain::on_close)
    EVT_TEXT_ENTER(ID_CommandInput, DebuggerMain::on_command_enter)
wxEND_EVENT_TABLE()

DebuggerMain::DebuggerMain(wxWindow* parent, DSystem* system, DebuggerReason reason, const std::string& message)
    : wxFrame(parent, wxID_ANY, "Darkstar Debugger", wxDefaultPosition, wxSize(800, 600))
    , system_(system)
    , reason_(reason)
    , entry_message_(message)
    , cp_debugger_(nullptr)
    , iop_debugger_(nullptr)
{
    auto* sizer = new wxBoxSizer(wxVERTICAL);

    output_text_ = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2 | wxHSCROLL);
    output_text_->SetFont(wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
    sizer->Add(output_text_, 1, wxEXPAND | wxALL, 2);

    command_input_ = new wxTextCtrl(this, ID_CommandInput, "", wxDefaultPosition, wxDefaultSize,
        wxTE_PROCESS_ENTER);
    command_input_->SetFont(wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
    sizer->Add(command_input_, 0, wxEXPAND | wxALL, 2);

    SetSizer(sizer);

    // Open child debugger windows
    cp_debugger_ = new CPDebugger(this, system);
    cp_debugger_->Show();

    iop_debugger_ = new IOPDebugger(this, system);
    iop_debugger_->Show();

    switch (reason_) {
        case DebuggerReason::UserInvoked:
            write_line(entry_message_);
            break;
        case DebuggerReason::Error:
            write_line(entry_message_);
            print_iop_status();
            print_cp_status();
            print_mesa_status();
            break;
    }

    command_input_->SetFocus();
}

DebuggerMain::~DebuggerMain() {
}

void DebuggerMain::write_line(const std::string& text) {
    output_text_->AppendText(wxString(text) + "\n");
}

void DebuggerMain::on_close(wxCloseEvent& event) {
    if (cp_debugger_) { cp_debugger_->Close(); cp_debugger_ = nullptr; }
    if (iop_debugger_) { iop_debugger_->Close(); iop_debugger_ = nullptr; }
    event.Skip();
}

void DebuggerMain::on_key_down(wxKeyEvent& event) {
    if (event.ControlDown() && event.GetKeyCode() == 'C' && system_->is_executing()) {
        write_line("*user break*");
        stop_execution();
        display_current_code();
    } else {
        event.Skip();
    }
}

void DebuggerMain::on_command_enter(wxCommandEvent& /*event*/) {
    std::string cmd = command_input_->GetValue().ToStdString();
    command_input_->Clear();

    if (!cmd.empty()) {
        write_line("> " + cmd);
        process_command(cmd);
    }
}

void DebuggerMain::process_command(const std::string& command) {
    std::istringstream iss(command);
    std::string cmd;
    iss >> cmd;

    // Convert to lowercase
    for (auto& c : cmd) c = static_cast<char>(std::tolower(c));

    if (cmd == "go" || cmd == "g") {
        start_execution();
    } else if (cmd == "stop" || cmd == "s") {
        stop_execution();
    } else if (cmd == "reset" || cmd == "r") {
        system_->reset();
        write_line("System reset.");
    } else if (cmd == "cp") {
        print_cp_status();
    } else if (cmd == "iop") {
        print_iop_status();
    } else if (cmd == "mesa") {
        print_mesa_status();
    } else if (cmd == "help" || cmd == "?") {
        write_line("Commands: go, stop, reset, cp, iop, mesa, help, quit");
    } else if (cmd == "quit" || cmd == "q") {
        Close();
    } else {
        write_line("Unknown command: " + cmd + " (type 'help' for commands)");
    }
}

void DebuggerMain::stop_execution() {
    if (system_->is_executing()) {
        system_->stop_execution();
        write_line("Execution stopped.");
        display_current_code();
    }
}

void DebuggerMain::start_execution() {
    if (!system_->is_executing()) {
        auto context = std::make_shared<SystemExecutionContext>(
            nullptr, nullptr, nullptr, nullptr);
        system_->start_execution(context);
        write_line("Execution started.");
    }
}

void DebuggerMain::display_current_code() {
    if (cp_debugger_) cp_debugger_->update_display();
    if (iop_debugger_) iop_debugger_->update_display();
}

void DebuggerMain::print_iop_status() {
    auto& iop = system_->iop();
    std::ostringstream oss;
    oss << "IOP Status:" << std::endl;
    oss << "  8085 PC=" << std::hex << std::setfill('0') << std::setw(4) << iop.cpu().pc()
        << " SP=" << std::setw(4) << iop.cpu().sp() << std::endl;
    oss << "  A=" << std::setw(2) << static_cast<int>(iop.cpu().a())
        << " BC=" << std::setw(4) << iop.cpu().bc()
        << " DE=" << std::setw(4) << iop.cpu().de()
        << " HL=" << std::setw(4) << iop.cpu().hl();
    write_line(oss.str());
}

void DebuggerMain::print_cp_status() {
    auto& cp = system_->cp();
    std::ostringstream oss;
    oss << "CP Status:" << std::endl;
    oss << "  Task=" << static_cast<int>(cp.current_task())
        << " TPC=" << std::hex << std::setfill('0') << std::setw(3) << cp.tpc()[static_cast<int>(cp.current_task())]
        << " Cycle=" << std::dec << cp.cycle();
    write_line(oss.str());
}

void DebuggerMain::print_mesa_status() {
    write_line("Mesa Status: (use CP debugger for detailed view)");
}

} // namespace darkstar
