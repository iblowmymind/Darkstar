/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.

    Emscripten glue layer – exports the Darkstar emulator API to JavaScript.
    All exported functions are prefixed with "darkstar_".
*/

#include <cstdint>
#include <cstdlib>
#include <cstring>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define EXPORT extern "C" EMSCRIPTEN_KEEPALIVE
#else
#define EXPORT extern "C"
#endif

#include "dsystem.h"
#include "iop/io_processor.h"
#include "iop/keyboard.h"
#include "iop/mouse.h"
#include "iop/floppy_drive.h"
#include "io/floppy_disk.h"

// ---------------------------------------------------------------------------
// Global emulator state
// ---------------------------------------------------------------------------
static DSystem*     g_system    = nullptr;
static IOProcessor* g_iop       = nullptr;
static FloppyDisk*  g_floppy    = nullptr;   // currently loaded floppy image
static uint64_t     g_instruction_count = 0; // total IOP instructions executed

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

/// Initialise the emulator.  Must be called once before any other function.
EXPORT void darkstar_init() {
    if (g_system) return;
    g_system = new DSystem();
    g_iop    = new IOProcessor(g_system);
    printf("[Darkstar] Emulator initialised. ROM[0]=0x%02X ROM[1]=0x%02X ROM[2]=0x%02X\n",
           g_iop->Memory()->ReadByte(0),
           g_iop->Memory()->ReadByte(1),
           g_iop->Memory()->ReadByte(2));
}

/// Hard reset the emulator (keeps loaded disk image).
EXPORT void darkstar_reset() {
    if (!g_system) darkstar_init();
    g_iop->Reset();
    g_system->Reset();
    g_instruction_count = 0;
}

/// Free all resources.
EXPORT void darkstar_destroy() {
    delete g_iop;    g_iop    = nullptr;
    delete g_system; g_system = nullptr;
    delete g_floppy; g_floppy = nullptr;
}

// ---------------------------------------------------------------------------
// Execution
// ---------------------------------------------------------------------------

/// Execute a single IOP instruction (and any scheduled events that fall due).
/// Returns the number of CPU cycles consumed.
EXPORT int darkstar_step() {
    if (!g_iop) return 0;
    int cycles = g_iop->Execute();
    g_instruction_count++;
    return cycles;
}

/// Execute the emulator for approximately `nsec` nanoseconds of emulated time.
/// Returns the actual number of IOP instructions executed.
EXPORT int darkstar_run_nsec(uint32_t nsec) {
    if (!g_iop || !g_system) return 0;
    int count = 0;
    Scheduler* sched = g_system->GetScheduler();
    // Rough: assume ~500 ns per IOP instruction (i8085 @ 2 MHz)
    int target = (int)(nsec / 500) + 1;
    for (int i = 0; i < target; i++) {
        g_iop->Execute();
        count++;
    }
    g_instruction_count += static_cast<uint64_t>(count);
    return count;
}

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------

/// Return a pointer to the RGBA pixel buffer (1088 × 860 × 4 bytes).
EXPORT const uint8_t* darkstar_get_display_ptr() {
    if (!g_system) return nullptr;
    DisplaySurface* surf = g_system->GetDisplay();
    return surf ? surf->Pixels() : nullptr;
}

EXPORT int darkstar_display_width()  { return DisplaySurface::DISPLAY_WIDTH_PIXELS;  }
EXPORT int darkstar_display_height() { return DisplaySurface::DISPLAY_HEIGHT_LINES;  }

// ---------------------------------------------------------------------------
// Keyboard
// ---------------------------------------------------------------------------

/// Key-down event.  keycode is a KeyCode enum value (uint8_t).
EXPORT void darkstar_key_down(uint8_t keycode) {
    if (!g_iop) return;
    g_iop->GetKeyboard()->KeyDown(static_cast<KeyCode>(keycode));
}

/// Key-up event.
EXPORT void darkstar_key_up(uint8_t keycode) {
    if (!g_iop) return;
    g_iop->GetKeyboard()->KeyUp(static_cast<KeyCode>(keycode));
}

// ---------------------------------------------------------------------------
// Mouse
// ---------------------------------------------------------------------------

/// Report mouse movement (relative deltas in pixels).
EXPORT void darkstar_mouse_move(int dx, int dy) {
    if (!g_iop) return;
    g_iop->GetMouse()->MouseMove(dx, dy);
}

/// Report a mouse button press.  button: 0=left, 1=middle, 2=right
EXPORT void darkstar_mouse_down(int button) {
    if (!g_iop) return;
    StarMouseButton b = StarMouseButton::None;
    switch (button) {
        case 0: b = StarMouseButton::Left;   break;
        case 1: b = StarMouseButton::Middle; break;
        case 2: b = StarMouseButton::Right;  break;
    }
    g_iop->GetMouse()->MouseDown(b);
}

/// Report a mouse button release.  button: 0=left, 1=middle, 2=right
EXPORT void darkstar_mouse_up(int button) {
    if (!g_iop) return;
    StarMouseButton b = StarMouseButton::None;
    switch (button) {
        case 0: b = StarMouseButton::Left;   break;
        case 1: b = StarMouseButton::Middle; break;
        case 2: b = StarMouseButton::Right;  break;
    }
    g_iop->GetMouse()->MouseUp(b);
}

// ---------------------------------------------------------------------------
// Disk image loading
// ---------------------------------------------------------------------------

/// Load a floppy disk image from a memory buffer (IMD or raw format).
/// data: pointer to image bytes, len: byte count.
/// Returns 1 on success, 0 on failure.
EXPORT int darkstar_load_floppy(const uint8_t* data, int len) {
    if (!g_iop) return 0;

    // Free any previously loaded image
    FloppyDrive* drive = g_iop->GetFloppyController()->Drive();
    drive->UnloadDisk();
    delete g_floppy;
    g_floppy = nullptr;

    if (!data || len <= 0) return 1; // just unloaded

    std::vector<uint8_t> buf(data, data + len);
    FloppyDisk* disk = new FloppyDisk(buf);
    if (!disk->IsLoaded()) { delete disk; return 0; }

    g_floppy = disk;
    drive->LoadDisk(g_floppy);
    return 1;
}

/// Eject the currently loaded floppy disk.
EXPORT void darkstar_eject_floppy() {
    if (!g_iop) return;
    g_iop->GetFloppyController()->Drive()->UnloadDisk();
    delete g_floppy;
    g_floppy = nullptr;
}

// ---------------------------------------------------------------------------
// Microcode / status (used by debugger and boot monitor)
// ---------------------------------------------------------------------------

/// Returns the current MP (maintenance panel) code from the memory controller
/// status register, or 0 if not available.
EXPORT uint16_t darkstar_get_mp_code() {
    if (!g_system) return 0;
    MemoryController* mc = g_system->GetMemoryController();
    return mc ? mc->MStatus() : 0;
}

/// Returns 1 if the display is currently on.
EXPORT int darkstar_display_on() {
    if (!g_system) return 0;
    DisplayController* dc = g_system->GetDisplayController();
    return (dc && dc->DisplayOn()) ? 1 : 0;
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

/// Returns the current program counter of the IOP (i8085) CPU.
EXPORT uint16_t darkstar_get_iop_pc() {
    if (!g_iop) return 0;
    return g_iop->CPU()->PC();
}

/// Returns 1 if the IOP (i8085) CPU is halted, 0 otherwise.
EXPORT int darkstar_is_iop_halted() {
    if (!g_iop) return 0;
    return g_iop->CPU()->Halted() ? 1 : 0;
}

/// Returns the total number of IOP instructions executed since init/reset.
EXPORT uint32_t darkstar_get_instruction_count() {
    return static_cast<uint32_t>(g_instruction_count);
}
