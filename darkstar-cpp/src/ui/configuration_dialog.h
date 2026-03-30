/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/spinctrl.h>

namespace darkstar {

class ConfigurationDialog : public wxDialog {
public:
    explicit ConfigurationDialog(wxWindow* parent);

    void apply_to_config();

private:
    void create_system_page(wxNotebook* notebook);
    void create_ethernet_page(wxNotebook* notebook);
    void create_display_page(wxNotebook* notebook);
    void create_time_page(wxNotebook* notebook);

    void populate_from_config();

    // System page
    wxChoice* memory_size_choice_ = nullptr;
    wxTextCtrl* host_id_text_ = nullptr;
    wxCheckBox* throttle_speed_check_ = nullptr;

    // Ethernet page
    wxTextCtrl* nethub_host_text_ = nullptr;
    wxSpinCtrl* nethub_port_spin_ = nullptr;

    // Display page
    wxChoice* display_scale_choice_ = nullptr;
    wxCheckBox* slow_phosphor_check_ = nullptr;
    wxCheckBox* full_screen_stretch_check_ = nullptr;

    // Time page
    wxRadioButton* tod_host_y2k_radio_ = nullptr;
    wxRadioButton* tod_host_time_radio_ = nullptr;
    wxRadioButton* tod_specific_radio_ = nullptr;
    wxRadioButton* tod_no_change_radio_ = nullptr;
};

} // namespace darkstar
