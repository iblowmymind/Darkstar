/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <wx/wx.h>
#include <memory>

namespace darkstar {

class DSystem;
class DisplayPanel;

class MainWindow : public wxFrame {
public:
    MainWindow(DSystem* system);
    ~MainWindow() override;

    DisplayPanel* display_panel() { return display_panel_; }
    int frame_count() const;

private:
    void create_menus();
    void create_status_bar();
    void update_ui_run_state();
    void update_mp_code();
    void update_hard_drive_label();
    void update_floppy_drive_label();
    void update_mouse_state();

    // Menu event handlers
    void on_start_stop(wxCommandEvent& event);
    void on_reset(wxCommandEvent& event);
    void on_floppy_load(wxCommandEvent& event);
    void on_floppy_unload(wxCommandEvent& event);
    void on_floppy_write_protect(wxCommandEvent& event);
    void on_hard_disk_load(wxCommandEvent& event);
    void on_new_hard_disk_sa1004(wxCommandEvent& event);
    void on_new_hard_disk_q2040(wxCommandEvent& event);
    void on_new_hard_disk_q2080(wxCommandEvent& event);
    void on_configuration(wxCommandEvent& event);
    void on_full_screen(wxCommandEvent& event);
    void on_exit(wxCommandEvent& event);
    void on_about(wxCommandEvent& event);
    void on_alt_boot(wxCommandEvent& event);

    // Timer
    void on_frame_timer(wxTimerEvent& event);

    // Window events
    void on_close(wxCloseEvent& event);
    void on_shown(wxShowEvent& event);

    // Error callback from emulation
    void on_execution_error(const std::exception& e);

    std::string show_load_dialog(bool floppy);
    std::string show_save_dialog(bool floppy);

    DSystem* system_;
    DisplayPanel* display_panel_;
    wxTimer frame_timer_;

    // Menu items we need to update
    wxMenuItem* start_stop_item_ = nullptr;
    wxMenuItem* floppy_write_protect_item_ = nullptr;
    wxMenuItem* hard_disk_label_item_ = nullptr;
    wxMenuItem* floppy_label_item_ = nullptr;
    wxMenu* alt_boot_menu_ = nullptr;

    // Status bar field indices
    enum StatusFields {
        kFieldMP = 0,
        kFieldExecution,
        kFieldFPS,
        kFieldMouse,
        kFieldCount
    };

    bool first_shown_ = true;
    bool full_screen_ = false;

    // Menu IDs
    enum {
        ID_StartStop = wxID_HIGHEST + 1,
        ID_Reset,
        ID_FloppyLoad,
        ID_FloppyUnload,
        ID_FloppyWriteProtect,
        ID_HardDiskLoad,
        ID_NewHardDiskSA1004,
        ID_NewHardDiskQ2040,
        ID_NewHardDiskQ2080,
        ID_Configuration,
        ID_FullScreen,
        ID_AltBootBase = wxID_HIGHEST + 100,  // Reserve 100-110 for alt boot
    };
};

class DarkstarApp : public wxApp {
public:
    bool OnInit() override;
    int OnExit() override;

private:
    std::unique_ptr<DSystem> system_;
    MainWindow* main_window_ = nullptr;
};

} // namespace darkstar
