/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#include "debugger/cp_debugger.h"
#include "core/system.h"
#include "cp/central_processor.h"
#include "cp/microinstruction.h"
#include "cp/am2901.h"

#include <iomanip>
#include <sstream>

namespace darkstar {

wxBEGIN_EVENT_TABLE(CPDebugger, wxFrame)
    EVT_CLOSE(CPDebugger::on_close)
wxEND_EVENT_TABLE()

CPDebugger::CPDebugger(wxWindow* parent, DSystem* system)
    : wxFrame(parent, wxID_ANY, "CP Debugger", wxDefaultPosition, wxSize(700, 500))
    , system_(system)
{
    create_controls();
    update_display();
}

void CPDebugger::create_controls() {
    auto* main_sizer = new wxBoxSizer(wxVERTICAL);

    // Register display panel
    auto* reg_panel = new wxPanel(this);
    auto* reg_sizer = new wxFlexGridSizer(2, 6, 4, 8);

    reg_sizer->Add(new wxStaticText(reg_panel, wxID_ANY, "Task:"), 0, wxALIGN_RIGHT);
    task_label_ = new wxStaticText(reg_panel, wxID_ANY, "---");
    reg_sizer->Add(task_label_);

    reg_sizer->Add(new wxStaticText(reg_panel, wxID_ANY, "TPC:"), 0, wxALIGN_RIGHT);
    tpc_label_ = new wxStaticText(reg_panel, wxID_ANY, "---");
    reg_sizer->Add(tpc_label_);

    reg_sizer->Add(new wxStaticText(reg_panel, wxID_ANY, "Cycle:"), 0, wxALIGN_RIGHT);
    cycle_label_ = new wxStaticText(reg_panel, wxID_ANY, "---");
    reg_sizer->Add(cycle_label_);

    reg_sizer->Add(new wxStaticText(reg_panel, wxID_ANY, "ALU:"), 0, wxALIGN_RIGHT);
    alu_label_ = new wxStaticText(reg_panel, wxID_ANY, "---");
    reg_sizer->Add(alu_label_);

    reg_sizer->Add(new wxStaticText(reg_panel, wxID_ANY, "StackP:"), 0, wxALIGN_RIGHT);
    stackp_label_ = new wxStaticText(reg_panel, wxID_ANY, "---");
    reg_sizer->Add(stackp_label_);

    reg_sizer->Add(new wxStaticText(reg_panel, wxID_ANY, "IB:"), 0, wxALIGN_RIGHT);
    ib_label_ = new wxStaticText(reg_panel, wxID_ANY, "---");
    reg_sizer->Add(ib_label_);

    reg_panel->SetSizer(reg_sizer);
    main_sizer->Add(reg_panel, 0, wxEXPAND | wxALL, 4);

    // Microcode list
    microcode_list_ = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxLC_REPORT | wxLC_VIRTUAL | wxLC_SINGLE_SEL);
    microcode_list_->AppendColumn("Addr", wxLIST_FORMAT_LEFT, 60);
    microcode_list_->AppendColumn("Disassembly", wxLIST_FORMAT_LEFT, 500);
    main_sizer->Add(microcode_list_, 1, wxEXPAND | wxALL, 4);

    // U and RH registers
    auto* bottom_sizer = new wxBoxSizer(wxHORIZONTAL);

    u_regs_text_ = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_READONLY);
    u_regs_text_->SetFont(wxFont(9, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
    bottom_sizer->Add(u_regs_text_, 1, wxEXPAND | wxRIGHT, 2);

    rh_regs_text_ = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_READONLY);
    rh_regs_text_->SetFont(wxFont(9, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
    bottom_sizer->Add(rh_regs_text_, 1, wxEXPAND | wxLEFT, 2);

    main_sizer->Add(bottom_sizer, 0, wxEXPAND | wxALL, 4);

    SetSizer(main_sizer);
}

void CPDebugger::update_display() {
    update_registers();
    update_microcode_view();
}

void CPDebugger::update_registers() {
    auto& cp = system_->cp();

    static const char* task_names[] = {
        "Emulator", "Display", "Ethernet", "Refresh", "Disk", "IOP", "IOPcs", "Kernel"
    };

    int task = static_cast<int>(cp.current_task());
    task_label_->SetLabel(wxString::Format("%s (%d)", task_names[task], task));

    int tpc = cp.tpc()[task];
    tpc_label_->SetLabel(wxString::Format("0x%03x", tpc));
    cycle_label_->SetLabel(wxString::Format("C%d", cp.cycle()));
    stackp_label_->SetLabel(wxString::Format("%d", cp.stack_p()));

    std::ostringstream ib_oss;
    ib_oss << "Ptr=" << static_cast<int>(cp.ib_ptr())
           << " Front=0x" << std::hex << std::setfill('0') << std::setw(2)
           << static_cast<int>(cp.ib_front());
    ib_label_->SetLabel(ib_oss.str());

    auto& alu = cp.alu();
    std::ostringstream alu_oss;
    alu_oss << "Y=0x" << std::hex << std::setfill('0') << std::setw(4) << alu.y();
    alu_label_->SetLabel(alu_oss.str());

    // Update U registers
    std::ostringstream u_oss;
    u_oss << "U Registers:\n";
    const uint16_t* u = cp.u();
    for (int i = 0; i < 16; i++) {
        u_oss << "  U[" << std::dec << std::setw(2) << i << "] = 0x"
              << std::hex << std::setfill('0') << std::setw(4) << u[i] << "\n";
    }
    u_regs_text_->SetValue(u_oss.str());

    // Update RH registers
    std::ostringstream rh_oss;
    rh_oss << "RH Registers:\n";
    const uint8_t* rh = cp.rh();
    for (int i = 0; i < 16; i++) {
        rh_oss << "  RH[" << std::dec << std::setw(2) << i << "] = 0x"
               << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(rh[i]) << "\n";
    }
    rh_regs_text_->SetValue(rh_oss.str());
}

void CPDebugger::update_microcode_view() {
    // Show a window of microcode around the current TPC
    auto& cp = system_->cp();
    int task = static_cast<int>(cp.current_task());
    int current_tpc = cp.tpc()[task];

    microcode_list_->DeleteAllItems();

    int start = std::max(0, current_tpc - 10);
    int end = std::min(4095, current_tpc + 20);

    for (int addr = start; addr <= end; addr++) {
        long idx = microcode_list_->InsertItem(microcode_list_->GetItemCount(),
            wxString::Format("0x%03x", addr));

        uint64_t word = cp.microcode_ram()[addr];
        Microinstruction mi(word);
        std::string disasm = mi.disassemble(addr);
        microcode_list_->SetItem(idx, 1, disasm);

        if (addr == current_tpc) {
            microcode_list_->SetItemBackgroundColour(idx, wxColour(255, 255, 200));
        }
    }
}

void CPDebugger::on_close(wxCloseEvent& event) {
    event.Skip();
}

} // namespace darkstar
