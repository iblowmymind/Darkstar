/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/
#include "ui/main_window.h"

#include "core/configuration.h"
#include "core/system.h"
#include "io/floppy_disk.h"
#include "io/floppy_drive.h"
#include "io/sa1000.h"
#include "iop/floppy_controller.h"
#include "iop/io_processor.h"
#include "iop/misc_io.h"
#include "ui/about_dialog.h"
#include "ui/configuration_dialog.h"
#include "ui/display_panel.h"

#include <wx/filename.h>
#include <iostream>

namespace darkstar {

// ============================================================
// MainWindow
// ============================================================

MainWindow::MainWindow(DSystem* system)
    : wxFrame(nullptr, wxID_ANY, "Darkstar",
              wxDefaultPosition, wxDefaultSize,
              wxDEFAULT_FRAME_STYLE & ~wxRESIZE_BORDER)
    , system_(system)
    , frame_timer_(this) {

    display_panel_ = new DisplayPanel(this, system_);
    display_panel_->set_display_scale(Configuration::display_scale);

    create_menus();
    create_status_bar();

    // Set up system callbacks to post to UI thread
    system_->on_execution_state_changed = [this]() {
        CallAfter([this]() { update_ui_run_state(); });
    };

    // Frame timer for FPS display (1 second interval)
    frame_timer_.Start(1000);
    Bind(wxEVT_TIMER, &MainWindow::on_frame_timer, this);

    // Window events
    Bind(wxEVT_CLOSE_WINDOW, &MainWindow::on_close, this);
    Bind(wxEVT_SHOW, &MainWindow::on_shown, this);

    // Load configured disks
    if (!Configuration::floppy_drive_image.empty()) {
        try {
            auto disk = std::make_unique<FloppyDisk>(Configuration::floppy_drive_image);
            system_->iop().floppy_controller().drive().load_disk(std::move(disk));
        } catch (const std::exception& e) {
            std::cerr << "Unable to load floppy image: " << e.what() << std::endl;
        }
    }

    if (!Configuration::hard_drive_image.empty()) {
        try {
            system_->hard_drive().load(Configuration::hard_drive_image);
        } catch (const std::exception& e) {
            std::cerr << "Unable to load hard drive image: " << e.what() << std::endl;
        }
    }

    update_ui_run_state();
    update_hard_drive_label();
    update_floppy_drive_label();
    update_mouse_state();

    Fit();
    Centre();
}

MainWindow::~MainWindow() = default;

void MainWindow::create_menus() {
    auto* menu_bar = new wxMenuBar();

    // System menu
    auto* system_menu = new wxMenu();
    start_stop_item_ = system_menu->Append(ID_StartStop, "Start");
    system_menu->Append(ID_Reset, "Reset");
    system_menu->AppendSeparator();

    // Alt Boot submenu
    alt_boot_menu_ = new wxMenu();
    const char* boot_names[] = {
        "None", "DiagnosticRigid", "Rigid", "Floppy", "Ethernet",
        "DiagnosticEthernet", "DiagnosticFloppy", "AlternateEthernet",
        "DiagnosticTrident1", "DiagnosticTrident2", "DiagnosticTrident3"
    };
    for (int i = 0; i <= 10; i++) {
        auto* item = alt_boot_menu_->AppendRadioItem(
            ID_AltBootBase + i + 1, boot_names[i]);
        if (i == static_cast<int>(Configuration::alt_boot_mode) + 1 ||
            (Configuration::alt_boot_mode == AltBootValues::None && i == 0)) {
            item->Check(true);
        }
    }
    system_menu->AppendSubMenu(alt_boot_menu_, "Alternate Boot");
    system_menu->AppendSeparator();

    // Floppy submenu
    auto* floppy_menu = new wxMenu();
    floppy_label_item_ = floppy_menu->Append(wxID_ANY, "No Floppy Loaded");
    floppy_label_item_->Enable(false);
    floppy_menu->AppendSeparator();
    floppy_menu->Append(ID_FloppyLoad, "Load Floppy...");
    floppy_menu->Append(ID_FloppyUnload, "Unload Floppy");
    floppy_write_protect_item_ = floppy_menu->AppendCheckItem(
        ID_FloppyWriteProtect, "Write Protect");
    system_menu->AppendSubMenu(floppy_menu, "Floppy Disk");

    // Hard disk submenu
    auto* hd_menu = new wxMenu();
    hard_disk_label_item_ = hd_menu->Append(wxID_ANY, "No Hard Disk");
    hard_disk_label_item_->Enable(false);
    hd_menu->AppendSeparator();
    hd_menu->Append(ID_HardDiskLoad, "Load Hard Disk...");
    auto* new_hd_menu = new wxMenu();
    new_hd_menu->Append(ID_NewHardDiskSA1004, "SA1004 (10MB)");
    new_hd_menu->Append(ID_NewHardDiskQ2040, "Q2040 (40MB)");
    new_hd_menu->Append(ID_NewHardDiskQ2080, "Q2080 (80MB)");
    hd_menu->AppendSubMenu(new_hd_menu, "New Hard Disk");
    system_menu->AppendSubMenu(hd_menu, "Hard Disk");

    system_menu->AppendSeparator();
    system_menu->Append(ID_Configuration, "Configuration...");
    system_menu->Append(ID_FullScreen, "Full Screen\tF11");
    system_menu->AppendSeparator();
    system_menu->Append(ID_Exit, "Exit");

    menu_bar->Append(system_menu, "System");

    // Help menu
    auto* help_menu = new wxMenu();
    help_menu->Append(wxID_ABOUT, "About Darkstar...");
    menu_bar->Append(help_menu, "Help");

    SetMenuBar(menu_bar);

    // Bind events
    Bind(wxEVT_MENU, &MainWindow::on_start_stop, this, ID_StartStop);
    Bind(wxEVT_MENU, &MainWindow::on_reset, this, ID_Reset);
    Bind(wxEVT_MENU, &MainWindow::on_floppy_load, this, ID_FloppyLoad);
    Bind(wxEVT_MENU, &MainWindow::on_floppy_unload, this, ID_FloppyUnload);
    Bind(wxEVT_MENU, &MainWindow::on_floppy_write_protect, this, ID_FloppyWriteProtect);
    Bind(wxEVT_MENU, &MainWindow::on_hard_disk_load, this, ID_HardDiskLoad);
    Bind(wxEVT_MENU, &MainWindow::on_new_hard_disk_sa1004, this, ID_NewHardDiskSA1004);
    Bind(wxEVT_MENU, &MainWindow::on_new_hard_disk_q2040, this, ID_NewHardDiskQ2040);
    Bind(wxEVT_MENU, &MainWindow::on_new_hard_disk_q2080, this, ID_NewHardDiskQ2080);
    Bind(wxEVT_MENU, &MainWindow::on_configuration, this, ID_Configuration);
    Bind(wxEVT_MENU, &MainWindow::on_full_screen, this, ID_FullScreen);
    Bind(wxEVT_MENU, &MainWindow::on_exit, this, ID_Exit);
    Bind(wxEVT_MENU, &MainWindow::on_about, this, wxID_ABOUT);

    for (int i = 0; i <= 10; i++) {
        Bind(wxEVT_MENU, &MainWindow::on_alt_boot, this, ID_AltBootBase + i + 1);
    }
}

void MainWindow::create_status_bar() {
    auto* sb = CreateStatusBar(kFieldCount);
    int widths[] = { 120, 200, 200, -1 };
    sb->SetStatusWidths(kFieldCount, widths);
    SetStatusText("MP: ----", kFieldMP);
    SetStatusText("System is stopped.", kFieldExecution);
    SetStatusText("0 Fields/Sec", kFieldFPS);
    SetStatusText("Click on display to capture mouse/keyboard.", kFieldMouse);
}

void MainWindow::on_start_stop(wxCommandEvent&) {
    if (!system_->is_executing()) {
        auto context = std::make_shared<SystemExecutionContext>(
            nullptr, nullptr, nullptr,
            [this](const std::exception& e) { on_execution_error(e); });
        system_->start_execution(context);
    } else {
        system_->stop_execution();
    }
}

void MainWindow::on_reset(wxCommandEvent&) {
    system_->reset();
}

void MainWindow::on_floppy_load(wxCommandEvent&) {
    std::string path = show_load_dialog(true);
    if (!path.empty()) {
        system_->iop().floppy_controller().drive().unload_disk();
        try {
            auto disk = std::make_unique<FloppyDisk>(path);
            system_->iop().floppy_controller().drive().load_disk(std::move(disk));
            Configuration::floppy_drive_image = path;
            if (floppy_write_protect_item_->IsChecked()) {
                system_->iop().floppy_controller().drive().disk()->set_write_protected(true);
            }
        } catch (const std::exception& e) {
            wxMessageBox(wxString::Format("Unable to load floppy: %s", e.what()),
                         "Error", wxOK | wxICON_ERROR);
        }
        update_floppy_drive_label();
    }
}

void MainWindow::on_floppy_unload(wxCommandEvent&) {
    system_->iop().floppy_controller().drive().unload_disk();
    Configuration::floppy_drive_image.clear();
    update_floppy_drive_label();
}

void MainWindow::on_floppy_write_protect(wxCommandEvent&) {
    bool checked = floppy_write_protect_item_->IsChecked();
    auto* disk = system_->iop().floppy_controller().drive().disk();
    if (disk) {
        disk->set_write_protected(checked);
    }
}

void MainWindow::on_hard_disk_load(wxCommandEvent&) {
    std::string path = show_load_dialog(false);
    if (!path.empty()) {
        bool was_running = system_->is_executing();
        auto context = system_->is_executing() ? nullptr : nullptr; // simplified

        if (was_running) system_->stop_execution();

        try {
            system_->hard_drive().save();
        } catch (const std::exception& e) {
            if (wxMessageBox(
                    wxString::Format("Unable to save current hard drive: %s. Continue?", e.what()),
                    "Error", wxYES_NO | wxICON_WARNING) != wxYES) {
                if (was_running) {
                    auto ctx = std::make_shared<SystemExecutionContext>(
                        nullptr, nullptr, nullptr,
                        [this](const std::exception& ex) { on_execution_error(ex); });
                    system_->start_execution(ctx);
                }
                return;
            }
        }

        try {
            system_->hard_drive().load(path);
            Configuration::hard_drive_image = path;
        } catch (const std::exception& e) {
            wxMessageBox(wxString::Format("Unable to load drive image: %s", e.what()),
                         "Error", wxOK | wxICON_ERROR);
        }

        if (was_running) {
            auto ctx = std::make_shared<SystemExecutionContext>(
                nullptr, nullptr, nullptr,
                [this](const std::exception& ex) { on_execution_error(ex); });
            system_->start_execution(ctx);
        }

        update_hard_drive_label();
    }
}

void MainWindow::on_new_hard_disk_sa1004(wxCommandEvent&) {
    std::string path = show_save_dialog(false);
    if (!path.empty()) {
        system_->hard_drive().new_disk(DriveType::SA1004, path);
        update_hard_drive_label();
    }
}

void MainWindow::on_new_hard_disk_q2040(wxCommandEvent&) {
    std::string path = show_save_dialog(false);
    if (!path.empty()) {
        system_->hard_drive().new_disk(DriveType::Q2040, path);
        update_hard_drive_label();
    }
}

void MainWindow::on_new_hard_disk_q2080(wxCommandEvent&) {
    std::string path = show_save_dialog(false);
    if (!path.empty()) {
        system_->hard_drive().new_disk(DriveType::Q2080, path);
        update_hard_drive_label();
    }
}

void MainWindow::on_alt_boot(wxCommandEvent& event) {
    int index = event.GetId() - ID_AltBootBase - 1;
    AltBootValues val = (index == 0) ? AltBootValues::None
                                     : static_cast<AltBootValues>(index - 1);
    system_->iop().misc_io().set_alt_boot(val);
}

void MainWindow::on_configuration(wxCommandEvent&) {
    ConfigurationDialog dialog(this);
    if (dialog.ShowModal() == wxID_OK) {
        dialog.apply_to_config();
        display_panel_->set_display_scale(Configuration::display_scale);
        display_panel_->update_slow_phosphor();
    }
}

void MainWindow::on_full_screen(wxCommandEvent&) {
    full_screen_ = !full_screen_;
    ShowFullScreen(full_screen_);
    if (full_screen_) {
        display_panel_->capture_mouse_input();
    }
}

void MainWindow::on_exit(wxCommandEvent&) {
    Close(false);
}

void MainWindow::on_about(wxCommandEvent&) {
    AboutDialog dialog(this);
    dialog.ShowModal();
}

void MainWindow::on_frame_timer(wxTimerEvent&) {
    int count = display_panel_->frame_count_;
    display_panel_->frame_count_ = 0;

    SetStatusText(wxString::Format("%d Fields/Sec (%d%%)",
                                   count,
                                   static_cast<int>((count / 77.4) * 100.0)),
                  kFieldFPS);
}

void MainWindow::on_close(wxCloseEvent& event) {
    system_->stop_execution();
    Configuration::write_configuration();
    system_->shutdown(true);
    Destroy();
}

void MainWindow::on_shown(wxShowEvent& event) {
    if (first_shown_ && event.IsShown()) {
        first_shown_ = false;
        if (Configuration::start && !system_->is_executing()) {
            auto context = std::make_shared<SystemExecutionContext>(
                nullptr, nullptr, nullptr,
                [this](const std::exception& e) { on_execution_error(e); });
            system_->start_execution(context);
        }
    }
}

void MainWindow::on_execution_error(const std::exception& e) {
    CallAfter([this, msg = std::string(e.what())]() {
        wxMessageBox(wxString::Format("Execution error: %s", msg),
                     "Error", wxOK | wxICON_ERROR);
    });
}

void MainWindow::update_ui_run_state() {
    if (start_stop_item_) {
        start_stop_item_->SetItemLabel(
            system_->is_executing() ? "Stop" : "Start");
    }
    SetStatusText(system_->is_executing() ?
                  "System is running." : "System is stopped.",
                  kFieldExecution);
}

void MainWindow::update_mp_code() {
    if (system_->iop().misc_io().mp_panel_blank()) {
        SetStatusText("MP: ----", kFieldMP);
    } else {
        SetStatusText(wxString::Format("MP: %04d",
                                       system_->iop().misc_io().mp_panel_value()),
                      kFieldMP);
    }
}

void MainWindow::update_hard_drive_label() {
    if (hard_disk_label_item_) {
        wxFileName fn(system_->hard_drive().image_path());
        hard_disk_label_item_->SetItemLabel(
            wxString::Format("%s (%d)", fn.GetFullName(),
                             static_cast<int>(system_->hard_drive().type())));
    }
}

void MainWindow::update_floppy_drive_label() {
    if (floppy_label_item_) {
        if (system_->iop().floppy_controller().drive().is_loaded()) {
            wxFileName fn(system_->iop().floppy_controller().drive().disk()->image_path());
            floppy_label_item_->SetItemLabel(fn.GetFullName());
        } else {
            floppy_label_item_->SetItemLabel("No Floppy Loaded");
        }
    }
}

void MainWindow::update_mouse_state() {
    SetStatusText(display_panel_->is_mouse_captured() ?
                  "Mouse captured. Press Alt to release." :
                  "Click on display to capture mouse/keyboard.",
                  kFieldMouse);
}

std::string MainWindow::show_load_dialog(bool floppy) {
    wxString filter = floppy ?
        "Star Floppy Disk Images (*.imd)|*.imd|All Files (*.*)|*.*" :
        "Star Hard Disk Images (*.img)|*.img|All Files (*.*)|*.*";

    wxFileDialog dialog(this, "Select disk image to load", "", "",
                        filter, wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (dialog.ShowModal() == wxID_OK) {
        return dialog.GetPath().ToStdString();
    }
    return "";
}

std::string MainWindow::show_save_dialog(bool floppy) {
    wxString filter = floppy ?
        "Star Floppy Disk Images (*.imd)|*.imd|All Files (*.*)|*.*" :
        "Star Hard Disk Images (*.img)|*.img|All Files (*.*)|*.*";

    wxFileDialog dialog(this, "Select path for new disk image", "", "",
                        filter, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (dialog.ShowModal() == wxID_OK) {
        return dialog.GetPath().ToStdString();
    }
    return "";
}

int MainWindow::frame_count() const {
    return display_panel_->frame_count_;
}

// ============================================================
// DarkstarApp
// ============================================================

bool DarkstarApp::OnInit() {
    if (!wxApp::OnInit()) return false;

    // Process command-line arguments
    for (int i = 1; i < argc; i++) {
        wxString arg = argv[i];
        if (arg == "-config" && i + 1 < argc) {
            StartupOptions::configuration_file = argv[++i].ToStdString();
        } else if (arg == "-rompath" && i + 1 < argc) {
            StartupOptions::rom_path = argv[++i].ToStdString();
        } else if (arg == "-start") {
            StartupOptions::start = true;
        } else {
            std::cout << "Usage: darkstar [-config <configFile>] "
                      << "[-rompath <path>] [-start]" << std::endl;
            return false;
        }
    }

    std::cout << "Darkstar v1.2.0 (C++ wxWidgets port)" << std::endl;
    std::cout << "(c) 2017-2019 Living Computers: Museum+Labs" << std::endl;
    std::cout << std::endl;

    Configuration::read_configuration();

    // Initialize SDL for audio only
    // SDL_Init(SDL_INIT_AUDIO);  // Will be called by Beeper

    system_ = std::make_unique<DSystem>();
    system_->reset();

    main_window_ = new MainWindow(system_.get());
    system_->attach_display(main_window_->display_panel());
    main_window_->Show();

    return true;
}

int DarkstarApp::OnExit() {
    if (system_) {
        system_->stop_execution();
        system_->shutdown(true);
    }

    // SDL_Quit();  // Clean up SDL audio

    std::cout << "Goodbye..." << std::endl;
    return wxApp::OnExit();
}

} // namespace darkstar

wxIMPLEMENT_APP(darkstar::DarkstarApp);
