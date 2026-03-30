/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/
#include "ui/configuration_dialog.h"
#include "core/configuration.h"

#include <cstdio>

namespace darkstar {

ConfigurationDialog::ConfigurationDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, "Configuration",
               wxDefaultPosition, wxSize(450, 400)) {
    auto* notebook = new wxNotebook(this, wxID_ANY);

    create_system_page(notebook);
    create_ethernet_page(notebook);
    create_display_page(notebook);
    create_time_page(notebook);

    auto* button_sizer = CreateButtonSizer(wxOK | wxCANCEL);

    auto* main_sizer = new wxBoxSizer(wxVERTICAL);
    main_sizer->Add(notebook, 1, wxEXPAND | wxALL, 5);
    main_sizer->Add(button_sizer, 0, wxEXPAND | wxALL, 5);
    SetSizer(main_sizer);

    populate_from_config();
}

void ConfigurationDialog::create_system_page(wxNotebook* notebook) {
    auto* panel = new wxPanel(notebook);
    auto* sizer = new wxFlexGridSizer(2, 5, 10);
    sizer->AddGrowableCol(1);

    sizer->Add(new wxStaticText(panel, wxID_ANY, "Memory Size (KW):"),
               0, wxALIGN_CENTER_VERTICAL);
    memory_size_choice_ = new wxChoice(panel, wxID_ANY);
    memory_size_choice_->Append("128");
    memory_size_choice_->Append("256");
    memory_size_choice_->Append("384");
    memory_size_choice_->Append("512");
    memory_size_choice_->Append("640");
    memory_size_choice_->Append("768");
    sizer->Add(memory_size_choice_, 1, wxEXPAND);

    sizer->Add(new wxStaticText(panel, wxID_ANY, "Host ID (hex):"),
               0, wxALIGN_CENTER_VERTICAL);
    host_id_text_ = new wxTextCtrl(panel, wxID_ANY);
    sizer->Add(host_id_text_, 1, wxEXPAND);

    sizer->Add(new wxStaticText(panel, wxID_ANY, ""), 0);
    throttle_speed_check_ = new wxCheckBox(panel, wxID_ANY, "Throttle Speed");
    sizer->Add(throttle_speed_check_);

    panel->SetSizer(sizer);
    notebook->AddPage(panel, "System");
}

void ConfigurationDialog::create_ethernet_page(wxNotebook* notebook) {
    auto* panel = new wxPanel(notebook);
    auto* sizer = new wxFlexGridSizer(2, 5, 10);
    sizer->AddGrowableCol(1);

    sizer->Add(new wxStaticText(panel, wxID_ANY, "NetHub Host:"),
               0, wxALIGN_CENTER_VERTICAL);
    nethub_host_text_ = new wxTextCtrl(panel, wxID_ANY);
    sizer->Add(nethub_host_text_, 1, wxEXPAND);

    sizer->Add(new wxStaticText(panel, wxID_ANY, "NetHub Port:"),
               0, wxALIGN_CENTER_VERTICAL);
    nethub_port_spin_ = new wxSpinCtrl(panel, wxID_ANY, "",
                                        wxDefaultPosition, wxDefaultSize,
                                        wxSP_ARROW_KEYS, 1, 65535, 3333);
    sizer->Add(nethub_port_spin_, 1, wxEXPAND);

    panel->SetSizer(sizer);
    notebook->AddPage(panel, "Ethernet");
}

void ConfigurationDialog::create_display_page(wxNotebook* notebook) {
    auto* panel = new wxPanel(notebook);
    auto* sizer = new wxFlexGridSizer(2, 5, 10);
    sizer->AddGrowableCol(1);

    sizer->Add(new wxStaticText(panel, wxID_ANY, "Display Scale:"),
               0, wxALIGN_CENTER_VERTICAL);
    display_scale_choice_ = new wxChoice(panel, wxID_ANY);
    display_scale_choice_->Append("1x");
    display_scale_choice_->Append("2x");
    display_scale_choice_->Append("3x");
    display_scale_choice_->Append("4x");
    sizer->Add(display_scale_choice_, 1, wxEXPAND);

    sizer->Add(new wxStaticText(panel, wxID_ANY, ""), 0);
    slow_phosphor_check_ = new wxCheckBox(panel, wxID_ANY,
                                           "Slow Phosphor Effect");
    sizer->Add(slow_phosphor_check_);

    sizer->Add(new wxStaticText(panel, wxID_ANY, ""), 0);
    full_screen_stretch_check_ = new wxCheckBox(panel, wxID_ANY,
                                                 "Full Screen Stretch");
    sizer->Add(full_screen_stretch_check_);

    panel->SetSizer(sizer);
    notebook->AddPage(panel, "Display");
}

void ConfigurationDialog::create_time_page(wxNotebook* notebook) {
    auto* panel = new wxPanel(notebook);
    auto* sizer = new wxBoxSizer(wxVERTICAL);

    sizer->Add(new wxStaticText(panel, wxID_ANY, "TOD Clock Power-Up Mode:"),
               0, wxALL, 5);

    tod_host_y2k_radio_ = new wxRadioButton(panel, wxID_ANY,
        "Host Time - 28 Years (Y2K workaround)", wxDefaultPosition,
        wxDefaultSize, wxRB_GROUP);
    sizer->Add(tod_host_y2k_radio_, 0, wxALL, 3);

    tod_host_time_radio_ = new wxRadioButton(panel, wxID_ANY,
        "Current Host Time");
    sizer->Add(tod_host_time_radio_, 0, wxALL, 3);

    tod_specific_radio_ = new wxRadioButton(panel, wxID_ANY,
        "Specific Date and Time");
    sizer->Add(tod_specific_radio_, 0, wxALL, 3);

    tod_no_change_radio_ = new wxRadioButton(panel, wxID_ANY,
        "No Change");
    sizer->Add(tod_no_change_radio_, 0, wxALL, 3);

    panel->SetSizer(sizer);
    notebook->AddPage(panel, "Time");
}

void ConfigurationDialog::populate_from_config() {
    // Memory size
    int mem_index = (Configuration::memory_size / 128) - 1;
    if (mem_index >= 0 && mem_index < 6) {
        memory_size_choice_->SetSelection(mem_index);
    }

    // Host ID
    char hex_buf[32];
    std::snprintf(hex_buf, sizeof(hex_buf), "%012llx",
                  static_cast<unsigned long long>(Configuration::host_id));
    host_id_text_->SetValue(hex_buf);

    throttle_speed_check_->SetValue(Configuration::throttle_speed);

    // Ethernet
    nethub_host_text_->SetValue(Configuration::nethub_host);
    nethub_port_spin_->SetValue(Configuration::nethub_port);

    // Display
    int scale_index = Configuration::display_scale - 1;
    if (scale_index >= 0 && scale_index < 4) {
        display_scale_choice_->SetSelection(scale_index);
    }
    slow_phosphor_check_->SetValue(Configuration::slow_phosphor);
    full_screen_stretch_check_->SetValue(Configuration::full_screen_stretch);

    // Time
    switch (Configuration::tod_set_mode) {
        case TODPowerUpSetMode::HostTimeY2K:
            tod_host_y2k_radio_->SetValue(true); break;
        case TODPowerUpSetMode::HostTime:
            tod_host_time_radio_->SetValue(true); break;
        case TODPowerUpSetMode::SpecificDateAndTime:
        case TODPowerUpSetMode::SpecificDate:
            tod_specific_radio_->SetValue(true); break;
        case TODPowerUpSetMode::NoChange:
            tod_no_change_radio_->SetValue(true); break;
    }
}

void ConfigurationDialog::apply_to_config() {
    Configuration::memory_size = (memory_size_choice_->GetSelection() + 1) * 128;

    try {
        Configuration::host_id = std::stoull(
            host_id_text_->GetValue().ToStdString(), nullptr, 16);
    } catch (...) { /* keep current */ }

    Configuration::throttle_speed = throttle_speed_check_->GetValue();
    Configuration::nethub_host = nethub_host_text_->GetValue().ToStdString();
    Configuration::nethub_port = static_cast<uint16_t>(nethub_port_spin_->GetValue());
    Configuration::display_scale = display_scale_choice_->GetSelection() + 1;
    Configuration::slow_phosphor = slow_phosphor_check_->GetValue();
    Configuration::full_screen_stretch = full_screen_stretch_check_->GetValue();

    if (tod_host_y2k_radio_->GetValue())
        Configuration::tod_set_mode = TODPowerUpSetMode::HostTimeY2K;
    else if (tod_host_time_radio_->GetValue())
        Configuration::tod_set_mode = TODPowerUpSetMode::HostTime;
    else if (tod_specific_radio_->GetValue())
        Configuration::tod_set_mode = TODPowerUpSetMode::SpecificDateAndTime;
    else if (tod_no_change_radio_->GetValue())
        Configuration::tod_set_mode = TODPowerUpSetMode::NoChange;
}

} // namespace darkstar
