# Darkstar – Emscripten / WebAssembly build
#
# Prerequisites:
#   Emscripten SDK activated in PATH (emcc, em++)
#
# Usage:
#   make          – build web/darkstar.js + web/darkstar.wasm
#   make clean    – remove build artefacts
#   make native   – build a native (non-Wasm) debug binary  (requires g++)

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
SRC_DIR  := src
WEB_DIR  := web
BUILD_DIR := build

# ---------------------------------------------------------------------------
# Sources  (every .cpp except the emscripten bindings for native builds)
# ---------------------------------------------------------------------------
COMMON_SRCS := \
    $(SRC_DIR)/configuration.cpp \
    $(SRC_DIR)/display_controller.cpp \
    $(SRC_DIR)/dsystem.cpp \
    $(SRC_DIR)/memory.cpp \
    $(SRC_DIR)/scheduler.cpp \
    $(SRC_DIR)/cp/am2901.cpp \
    $(SRC_DIR)/cp/central_processor.cpp \
    $(SRC_DIR)/cp/control_store.cpp \
    $(SRC_DIR)/cp/cp_alu.cpp \
    $(SRC_DIR)/cp/cp_io.cpp \
    $(SRC_DIR)/cp/cp_mem.cpp \
    $(SRC_DIR)/cp/cp_nia.cpp \
    $(SRC_DIR)/io/floppy_disk.cpp \
    $(SRC_DIR)/io/sa1000.cpp \
    $(SRC_DIR)/io/shugart_controller.cpp \
    $(SRC_DIR)/iop/beeper.cpp \
    $(SRC_DIR)/iop/dma_controller.cpp \
    $(SRC_DIR)/iop/floppy_controller.cpp \
    $(SRC_DIR)/iop/i8085.cpp \
    $(SRC_DIR)/iop/io_processor.cpp \
    $(SRC_DIR)/iop/iop_io_bus.cpp \
    $(SRC_DIR)/iop/iop_memory_bus.cpp \
    $(SRC_DIR)/iop/keyboard.cpp \
    $(SRC_DIR)/iop/misc_io.cpp \
    $(SRC_DIR)/iop/mouse.cpp \
    $(SRC_DIR)/iop/printer.cpp \
    $(SRC_DIR)/iop/tod_clock.cpp

WASM_SRCS   := $(COMMON_SRCS) $(SRC_DIR)/emscripten_bindings.cpp

# ---------------------------------------------------------------------------
# Compiler flags
# ---------------------------------------------------------------------------
CXXFLAGS_COMMON := -std=c++17 -O2 -I$(SRC_DIR)

# Emscripten flags
EM_EXPORTED_FUNCTIONS := \
    _darkstar_init \
    _darkstar_reset \
    _darkstar_destroy \
    _darkstar_step \
    _darkstar_run_nsec \
    _darkstar_get_display_ptr \
    _darkstar_display_width \
    _darkstar_display_height \
    _darkstar_display_on \
    _darkstar_key_down \
    _darkstar_key_up \
    _darkstar_mouse_move \
    _darkstar_mouse_down \
    _darkstar_mouse_up \
    _darkstar_load_floppy \
    _darkstar_eject_floppy \
    _darkstar_get_mp_code \
    _malloc \
    _free

EM_EXPORTED_JSON := $(shell echo '["$(subst $() $(),",",$$(EM_EXPORTED_FUNCTIONS))"]' | sed 's/\\$$//')

EMCXXFLAGS := $(CXXFLAGS_COMMON) \
    -s WASM=1 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s EXPORTED_FUNCTIONS='[$(subst $() ,$(comma),$(foreach fn,$(EM_EXPORTED_FUNCTIONS),"$(fn)"))]' \
    -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","HEAPU8"]' \
    -s MODULARIZE=1 \
    -s EXPORT_NAME=DarkstarModule \
    -s ENVIRONMENT=web \
    --no-entry

comma := ,

# ---------------------------------------------------------------------------
# Default target – WebAssembly
# ---------------------------------------------------------------------------
.PHONY: all clean native web-dir

all: web-dir $(WEB_DIR)/darkstar.js

web-dir:
	mkdir -p $(WEB_DIR)

$(WEB_DIR)/darkstar.js: $(WASM_SRCS)
	em++ $(EMCXXFLAGS) \
	    -s EXPORTED_FUNCTIONS='[$(subst $() ,$(comma),$(foreach fn,$(EM_EXPORTED_FUNCTIONS),"$(fn)"))]' \
	    -o $@ \
	    $(WASM_SRCS)
	@echo "Build complete: $(WEB_DIR)/darkstar.js + $(WEB_DIR)/darkstar.wasm"

# ---------------------------------------------------------------------------
# Native debug build (g++)
# ---------------------------------------------------------------------------
native: $(BUILD_DIR)/darkstar_native

$(BUILD_DIR)/darkstar_native: $(COMMON_SRCS) | $(BUILD_DIR)
	g++ $(CXXFLAGS_COMMON) -o $@ $^

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# ---------------------------------------------------------------------------
clean:
	rm -f $(WEB_DIR)/darkstar.js $(WEB_DIR)/darkstar.wasm
	rm -rf $(BUILD_DIR)
