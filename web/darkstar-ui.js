/**
 * darkstar-ui.js
 * Browser-side glue between the Emscripten Darkstar module and the HTML UI.
 *
 * Assumes darkstar.js (the Emscripten-generated module) has already been
 * loaded and exposes window.DarkstarModule (MODULARIZE=1, EXPORT_NAME=DarkstarModule).
 *
 * KeyCode values match the C++ KeyCode enum in src/iop/keyboard.h.
 */
(function () {
  'use strict';

  // -------------------------------------------------------------------------
  // KeyCode table – JS KeyboardEvent.code → Star KeyCode (uint8_t)
  // -------------------------------------------------------------------------
  const KEY_MAP = {
    'KeyA': 0x5c, 'KeyB': 0x48, 'KeyC': 0x4a, 'KeyD': 0x5a, 'KeyE': 0x6a,
    'KeyF': 0x59, 'KeyG': 0x58, 'KeyH': 0x57, 'KeyI': 0x65, 'KeyJ': 0x56,
    'KeyK': 0x55, 'KeyL': 0x54, 'KeyM': 0x46, 'KeyN': 0x47, 'KeyO': 0x64,
    'KeyP': 0x63, 'KeyQ': 0x6c, 'KeyR': 0x69, 'KeyS': 0x5b, 'KeyT': 0x68,
    'KeyU': 0x66, 'KeyV': 0x49, 'KeyW': 0x6b, 'KeyX': 0x4b, 'KeyY': 0x67,
    'KeyZ': 0x4c,
    'Digit1': 0x7c, 'Digit2': 0x7b, 'Digit3': 0x7a, 'Digit4': 0x79,
    'Digit5': 0x78, 'Digit6': 0x77, 'Digit7': 0x76, 'Digit8': 0x75,
    'Digit9': 0x74, 'Digit0': 0x73,
    'Space':     0x36, 'Return':    0x50, 'Backspace': 0x70,
    'Tab':       0x6d, 'Minus':     0x72, 'Equal':     0x71,
    'BracketLeft': 0x62, 'BracketRight': 0x61,
    'Semicolon': 0x53, 'Quote':     0x52, 'Backquote': 0x51,
    'Comma':     0x45, 'Period':    0x44, 'Slash':     0x43,
    'ShiftLeft': 0x5f, 'ShiftRight': 0x42,
    'CapsLock':  0x5e,
    'ArrowUp':   0x60, // A10 maps to up
    'Delete':    0x3b,
    'F1':  0x2a, // L1
    'F4':  0x29, // L4
    'F8':  0x38, // L8
    'F10': 0x27, // L10
    'F12': 0x31, // R10
  };

  // -------------------------------------------------------------------------
  // Timing
  // -------------------------------------------------------------------------
  // Target ~60 frames/sec; each frame runs ~16 ms of emulated time.
  // We run the IOP at ~2 MHz ≈ 500 ns per instruction.
  // 16 ms / 500 ns = ~32 000 instructions per frame – we call darkstar_run_nsec.
  const FRAME_NS = 16_000_000; // 16 ms in nanoseconds

  // -------------------------------------------------------------------------
  // State
  // -------------------------------------------------------------------------
  let ds     = null;   // resolved Emscripten module
  let canvas = null;
  let ctx    = null;
  let imageData = null;

  let running   = false;
  let rafHandle = null;
  let lastFpsTime = 0;
  let frameCount  = 0;

  // -------------------------------------------------------------------------
  // DOM helpers
  // -------------------------------------------------------------------------
  const $ = id => document.getElementById(id);

  function setStatus(msg) {
    const el = $('disk-label');
    if (el) el.textContent = msg;
  }

  function updateFps(now) {
    frameCount++;
    if (now - lastFpsTime >= 1000) {
      const fps = Math.round(frameCount * 1000 / (now - lastFpsTime));
      $('fps-counter').textContent = fps + ' fps';
      frameCount = 0;
      lastFpsTime = now;
    }
  }

  function updateMpCode() {
    const mp = ds._darkstar_get_mp_code ? ds._darkstar_get_mp_code() : 0;
    $('mp-code').textContent = 'MP: ' + mp.toString(16).toUpperCase().padStart(4, '0');
  }

  // -------------------------------------------------------------------------
  // Rendering – copy Wasm pixel buffer → canvas
  // -------------------------------------------------------------------------
  function renderFrame() {
    const ptr = ds._darkstar_get_display_ptr();
    if (!ptr) return;

    const w = ds._darkstar_display_width();
    const h = ds._darkstar_display_height();

    // Re-create ImageData only if dimensions changed
    if (!imageData || imageData.width !== w || imageData.height !== h) {
      imageData = ctx.createImageData(w, h);
    }

    // View into Wasm heap
    const src = new Uint8ClampedArray(ds.HEAPU8.buffer, ptr, w * h * 4);
    imageData.data.set(src);
    ctx.putImageData(imageData, 0, 0);
  }

  // -------------------------------------------------------------------------
  // Main loop
  // -------------------------------------------------------------------------
  function mainLoop(now) {
    if (!running) return;

    // Run ~1 frame of emulated time
    ds._darkstar_run_nsec(FRAME_NS);

    // Render
    renderFrame();

    // Status
    updateFps(now);
    updateMpCode();

    rafHandle = requestAnimationFrame(mainLoop);
  }

  // -------------------------------------------------------------------------
  // Controls
  // -------------------------------------------------------------------------
  function start() {
    if (running) return;
    running = true;
    $('btn-start').disabled = true;
    $('btn-stop').disabled  = false;
    $('overlay-msg').classList.add('hidden');
    lastFpsTime = performance.now();
    frameCount  = 0;
    rafHandle   = requestAnimationFrame(mainLoop);
  }

  function stop() {
    running = false;
    if (rafHandle) { cancelAnimationFrame(rafHandle); rafHandle = null; }
    $('btn-start').disabled = false;
    $('btn-stop').disabled  = true;
    $('overlay-msg').classList.remove('hidden');
    $('fps-counter').textContent = '0 fps';
  }

  function reset() {
    ds._darkstar_reset();
    if (ctx) ctx.clearRect(0, 0, canvas.width, canvas.height);
  }

  // -------------------------------------------------------------------------
  // Floppy loading
  // -------------------------------------------------------------------------
  function loadFloppy(file) {
    const reader = new FileReader();
    reader.onload = function (e) {
      const bytes = new Uint8Array(e.target.result);
      const len   = bytes.length;

      // Allocate memory in Wasm heap, copy image, call loader
      const ptr = ds._malloc(len);
      ds.HEAPU8.set(bytes, ptr);
      const ok = ds._darkstar_load_floppy(ptr, len);
      ds._free(ptr);

      if (ok) {
        setStatus('Disk: ' + file.name);
        $('btn-eject').disabled = false;
      } else {
        setStatus('Error loading disk image.');
      }
    };
    reader.readAsArrayBuffer(file);
  }

  function ejectFloppy() {
    ds._darkstar_eject_floppy();
    setStatus('');
    $('btn-eject').disabled = true;
  }

  // -------------------------------------------------------------------------
  // Keyboard handling
  // -------------------------------------------------------------------------
  function handleKeyDown(e) {
    if (!running) return;
    const kc = KEY_MAP[e.code];
    if (kc !== undefined) {
      e.preventDefault();
      ds._darkstar_key_down(kc);
    }
  }

  function handleKeyUp(e) {
    if (!running) return;
    const kc = KEY_MAP[e.code];
    if (kc !== undefined) {
      e.preventDefault();
      ds._darkstar_key_up(kc);
    }
  }

  // -------------------------------------------------------------------------
  // Mouse handling
  // -------------------------------------------------------------------------
  let lastMouseX = -1, lastMouseY = -1;

  function canvasMouseMove(e) {
    if (!running) return;
    const rect = canvas.getBoundingClientRect();
    const scaleX = canvas.width  / rect.width;
    const scaleY = canvas.height / rect.height;
    const cx = Math.round((e.clientX - rect.left) * scaleX);
    const cy = Math.round((e.clientY - rect.top)  * scaleY);

    if (lastMouseX >= 0) {
      const dx = cx - lastMouseX;
      const dy = cy - lastMouseY;
      if (dx !== 0 || dy !== 0) ds._darkstar_mouse_move(dx, dy);
    }
    lastMouseX = cx;
    lastMouseY = cy;
  }

  function canvasMouseDown(e) {
    if (!running) return;
    canvas.focus();
    ds._darkstar_mouse_down(e.button);
    e.preventDefault();
  }

  function canvasMouseUp(e) {
    if (!running) return;
    ds._darkstar_mouse_up(e.button);
  }

  // -------------------------------------------------------------------------
  // Init
  // -------------------------------------------------------------------------
  function init(module) {
    ds = module;

    // Initialise emulator
    ds._darkstar_init();

    // Get canvas
    canvas = $('screen');
    ctx    = canvas.getContext('2d');

    // Sync canvas size to the emulator's display dimensions
    canvas.width  = ds._darkstar_display_width();
    canvas.height = ds._darkstar_display_height();

    // Wire toolbar buttons
    $('btn-start').addEventListener('click', start);
    $('btn-stop' ).addEventListener('click', stop);
    $('btn-reset').addEventListener('click', reset);
    $('btn-eject').addEventListener('click', ejectFloppy);

    // Floppy file picker
    $('floppy-input').addEventListener('change', function () {
      if (this.files && this.files[0]) loadFloppy(this.files[0]);
      this.value = ''; // allow re-selecting same file
    });

    // Keyboard
    canvas.addEventListener('keydown', handleKeyDown);
    canvas.addEventListener('keyup',   handleKeyUp);

    // Mouse
    canvas.addEventListener('mousemove',  canvasMouseMove);
    canvas.addEventListener('mousedown',  canvasMouseDown);
    canvas.addEventListener('mouseup',    canvasMouseUp);
    canvas.addEventListener('contextmenu', e => e.preventDefault());

    // Pointer-lock for clean mouse deltas (optional, enhances UX)
    canvas.addEventListener('click', () => {
      if (running && canvas.requestPointerLock) canvas.requestPointerLock();
    });
    document.addEventListener('pointerlockchange', () => {
      if (document.pointerLockElement === canvas) {
        // While locked, use movementX/Y directly
        canvas.removeEventListener('mousemove', canvasMouseMove);
        canvas.addEventListener('mousemove', lockedMouseMove);
      } else {
        canvas.removeEventListener('mousemove', lockedMouseMove);
        canvas.addEventListener('mousemove', canvasMouseMove);
        lastMouseX = lastMouseY = -1;
      }
    });
  }

  function lockedMouseMove(e) {
    if (!running) return;
    const dx = e.movementX || 0;
    const dy = e.movementY || 0;
    if (dx !== 0 || dy !== 0) ds._darkstar_mouse_move(dx, dy);
  }

  // -------------------------------------------------------------------------
  // Bootstrap – wait for the Emscripten module to be ready
  // -------------------------------------------------------------------------
  function bootstrap() {
    if (typeof DarkstarModule === 'undefined') {
      // darkstar.js not yet loaded; retry
      setTimeout(bootstrap, 100);
      return;
    }
    DarkstarModule().then(function (mod) {
      init(mod);
    }).catch(function (err) {
      console.error('Failed to load Darkstar Wasm module:', err);
      $('overlay-msg').innerHTML =
        '<strong style="color:#e94560">Failed to load emulator module.</strong><br>' +
        'Check the browser console for details.';
    });
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', bootstrap);
  } else {
    bootstrap();
  }
})();
