/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/
#include "ui/about_dialog.h"

namespace darkstar {

AboutDialog::AboutDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, "About Darkstar",
               wxDefaultPosition, wxSize(350, 200)) {
    auto* sizer = new wxBoxSizer(wxVERTICAL);

    sizer->Add(new wxStaticText(this, wxID_ANY, "Darkstar v1.2.0"),
               0, wxALL | wxALIGN_CENTER, 10);
    sizer->Add(new wxStaticText(this, wxID_ANY,
               "Xerox 8010/1108 Star Workstation Emulator"),
               0, wxALL | wxALIGN_CENTER, 5);
    sizer->Add(new wxStaticText(this, wxID_ANY,
               "(c) 2017-2019 Living Computers: Museum+Labs"),
               0, wxALL | wxALIGN_CENTER, 5);
    sizer->Add(new wxStaticText(this, wxID_ANY,
               "C++ wxWidgets port"),
               0, wxALL | wxALIGN_CENTER, 5);

    sizer->AddStretchSpacer();
    sizer->Add(CreateButtonSizer(wxOK), 0, wxEXPAND | wxALL, 10);

    SetSizer(sizer);
}

} // namespace darkstar
