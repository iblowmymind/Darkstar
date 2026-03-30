/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#include "debugger/iop_debugger.h"
#include "core/system.h"
#include "iop/io_processor.h"
#include "iop/i8085.h"

#include <iomanip>
#include <sstream>

namespace darkstar {

wxBEGIN_EVENT_TABLE(IOPDebugger, wxFrame)
    EVT_CLOSE(IOPDebugger::on_close)
wxEND_EVENT_TABLE()

IOPDebugger::IOPDebugger(wxWindow* parent, DSystem* system)
    : wxFrame(parent, wxID_ANY, "IOP Debugger (8085)", wxDefaultPosition, wxSize(600, 450))
    , system_(system)
{
    create_controls();
    update_display();
}

void IOPDebugger::create_controls() {
    auto* main_sizer = new wxBoxSizer(wxVERTICAL);

    // Register panel
    auto* reg_panel = new wxPanel(this);
    auto* reg_sizer = new wxFlexGridSizer(2, 8, 4, 8);

    reg_sizer->Add(new wxStaticText(reg_panel, wxID_ANY, "PC:"), 0, wxALIGN_RIGHT);
    pc_label_ = new wxStaticText(reg_panel, wxID_ANY, "----");
    reg_sizer->Add(pc_label_);

    reg_sizer->Add(new wxStaticText(reg_panel, wxID_ANY, "SP:"), 0, wxALIGN_RIGHT);
    sp_label_ = new wxStaticText(reg_panel, wxID_ANY, "----");
    reg_sizer->Add(sp_label_);

    reg_sizer->Add(new wxStaticText(reg_panel, wxID_ANY, "A:"), 0, wxALIGN_RIGHT);
    a_label_ = new wxStaticText(reg_panel, wxID_ANY, "--");
    reg_sizer->Add(a_label_);

    reg_sizer->Add(new wxStaticText(reg_panel, wxID_ANY, "Flags:"), 0, wxALIGN_RIGHT);
    flags_label_ = new wxStaticText(reg_panel, wxID_ANY, "--------");
    reg_sizer->Add(flags_label_);

    reg_sizer->Add(new wxStaticText(reg_panel, wxID_ANY, "BC:"), 0, wxALIGN_RIGHT);
    bc_label_ = new wxStaticText(reg_panel, wxID_ANY, "----");
    reg_sizer->Add(bc_label_);

    reg_sizer->Add(new wxStaticText(reg_panel, wxID_ANY, "DE:"), 0, wxALIGN_RIGHT);
    de_label_ = new wxStaticText(reg_panel, wxID_ANY, "----");
    reg_sizer->Add(de_label_);

    reg_sizer->Add(new wxStaticText(reg_panel, wxID_ANY, "HL:"), 0, wxALIGN_RIGHT);
    hl_label_ = new wxStaticText(reg_panel, wxID_ANY, "----");
    reg_sizer->Add(hl_label_);

    reg_sizer->Add(new wxStaticText(reg_panel, wxID_ANY, "Halted:"), 0, wxALIGN_RIGHT);
    halted_label_ = new wxStaticText(reg_panel, wxID_ANY, "No");
    reg_sizer->Add(halted_label_);

    reg_panel->SetSizer(reg_sizer);
    main_sizer->Add(reg_panel, 0, wxEXPAND | wxALL, 4);

    // Disassembly list
    disasm_list_ = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxLC_REPORT | wxLC_SINGLE_SEL);
    disasm_list_->AppendColumn("Addr", wxLIST_FORMAT_LEFT, 60);
    disasm_list_->AppendColumn("Hex", wxLIST_FORMAT_LEFT, 80);
    disasm_list_->AppendColumn("Disassembly", wxLIST_FORMAT_LEFT, 300);
    main_sizer->Add(disasm_list_, 1, wxEXPAND | wxALL, 4);

    // Memory view
    memory_text_ = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(-1, 100),
        wxTE_MULTILINE | wxTE_READONLY);
    memory_text_->SetFont(wxFont(9, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
    main_sizer->Add(memory_text_, 0, wxEXPAND | wxALL, 4);

    SetSizer(main_sizer);
}

void IOPDebugger::update_display() {
    update_registers();
    update_disassembly();
}

void IOPDebugger::update_registers() {
    auto& cpu = system_->iop().cpu();

    pc_label_->SetLabel(wxString::Format("0x%04X", cpu.pc()));
    sp_label_->SetLabel(wxString::Format("0x%04X", cpu.sp()));
    a_label_->SetLabel(wxString::Format("0x%02X", cpu.a()));

    std::string flags;
    flags += (cpu.f() & 0x80) ? "S" : "-";
    flags += (cpu.f() & 0x40) ? "Z" : "-";
    flags += "-";
    flags += (cpu.f() & 0x10) ? "A" : "-";
    flags += "-";
    flags += (cpu.f() & 0x04) ? "P" : "-";
    flags += "-";
    flags += (cpu.f() & 0x01) ? "C" : "-";
    flags_label_->SetLabel(flags);

    bc_label_->SetLabel(wxString::Format("0x%04X", cpu.bc()));
    de_label_->SetLabel(wxString::Format("0x%04X", cpu.de()));
    hl_label_->SetLabel(wxString::Format("0x%04X", cpu.hl()));
    halted_label_->SetLabel(cpu.halted() ? "Yes" : "No");
}

void IOPDebugger::update_disassembly() {
    auto& cpu = system_->iop().cpu();
    uint16_t pc = cpu.pc();

    disasm_list_->DeleteAllItems();

    uint16_t addr = (pc >= 16) ? pc - 16 : 0;
    for (int i = 0; i < 40 && addr < 0xFFFF; i++) {
        std::string disasm = cpu.disassemble(addr);

        long idx = disasm_list_->InsertItem(disasm_list_->GetItemCount(),
            wxString::Format("0x%04X", addr));

        // Show hex bytes (up to 3)
        disasm_list_->SetItem(idx, 2, disasm);

        if (addr == pc) {
            disasm_list_->SetItemBackgroundColour(idx, wxColour(255, 255, 200));
        }

        // Advance address based on instruction size
        // Simple heuristic: just advance by 1 for display purposes
        addr++;
    }
}

void IOPDebugger::on_close(wxCloseEvent& event) {
    event.Skip();
}

} // namespace darkstar
