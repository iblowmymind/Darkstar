/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <wx/wx.h>

namespace darkstar {

class AboutDialog : public wxDialog {
public:
    explicit AboutDialog(wxWindow* parent);
};

} // namespace darkstar
