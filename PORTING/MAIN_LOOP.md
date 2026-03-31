# Main Loop and Frontend Entry Point

## Current Architecture (Windows)

The Windows main loop in `src/Altirra/source/main.cpp` is:

```
WinMain()
    → Initialize COM, window classes, create main window
    → g_sim.Init()
    → Enter message loop:
        for(;;) {
            ATUIProcessMessages()     // Dispatch Win32 messages
            g_pATIdle(false)          // Run emulation in idle time
                → Frame timing via MsgWaitForMultipleObjects
                → g_sim.Advance(dropFrame)
                → Update display if frame changed
        }
    → g_sim.Shutdown()
```

Key characteristics:

- **Message-driven**: Emulation runs in the idle callback between Windows
  messages
- **Win32 timing**: Uses `MsgWaitForMultipleObjects` with a waitable timer
  for precise frame pacing
- **Global state**: `g_sim`, `g_pDisplay`, `g_pATIdle` are globals
- **Single-threaded**: Emulation and UI run on the same thread

## SDL3 Main Loop

The SDL3 frontend uses a straightforward frame-based loop:

```cpp
// src/AltirraSDL/source/main_sdl3.cpp

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include "simulator.h"
#include "display_sdl3.h"
#include "input_sdl3.h"
#include "ui_state.h"

int main(int argc, char *argv[]) {
    // Initialize SDL3
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | SDL_INIT_TIMER);

    SDL_Window *window = SDL_CreateWindow("Altirra",
        800, 600, SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);

    // Initialize Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    // Initialize emulator
    // Note: sim.Init() calls ATCreateAudioOutput() internally.
    // On the SDL3 build, the factory returns ATAudioOutputSDL3
    // (see AUDIO.md for how this is handled at build time).
    ATSimulator sim;
    sim.Init();

    // Create SDL3 display implementation and connect to GTIA
    VDVideoDisplaySDL3 display(window, renderer);
    sim.GetGTIA().SetVideoOutput(&display);

    // Initialize the native audio device (SDL3 audio stream)
    sim.GetAudioOutput()->InitNativeAudio();

    // Register callback for when a blocked Advance() can retry.
    // This fires when the display consumes a frame, unblocking GTIA.
    bool advanceUnblocked = false;
    sim.SetOnAdvanceUnblocked([&advanceUnblocked]() {
        advanceUnblocked = true;
    });

    // Set up JSON-based configuration (replaces Windows registry)
    // (Done in system library init, see SYSTEM.md)

    // Load ROM/disk from command line
    if (argc > 1) {
        ATMediaLoadContext ctx;
        sim.Load(argv[1], kATMediaWriteMode_VRWSafe, ctx);
    }

    sim.ColdReset();

    // Main loop
    bool running = true;
    uint64 lastFrameTime = SDL_GetPerformanceCounter();
    uint64 perfFreq = SDL_GetPerformanceFrequency();

    while (running) {
        // --- Event Processing ---
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);

            switch (event.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;

            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                if (!ImGui::GetIO().WantCaptureKeyboard)
                    HandleKeyEvent(sim, event);
                break;

            case SDL_EVENT_MOUSE_MOTION:
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (!ImGui::GetIO().WantCaptureMouse)
                    HandleMouseEvent(sim, event);
                break;

            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                HandleGamepadEvent(sim, event);
                break;

            case SDL_EVENT_DROP_FILE:
                HandleFileDrop(sim, event);
                break;
            }
        }

        // --- Emulation Advance ---
        if (sim.IsRunning() && !sim.IsPaused()) {
            advanceUnblocked = false;

            ATSimulator::AdvanceResult ar = sim.Advance(false);

            if (ar == ATSimulator::kAdvanceResult_WaitingForFrame) {
                // GTIA tried to submit a frame via PostBuffer() but the
                // display's frame queue was full. This means a frame was
                // already submitted and is waiting to be presented.
                // The SetOnAdvanceUnblocked callback will fire when the
                // display consumes the frame. We present it now.
            }
        }

        // --- Rendering ---
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Render emulator frame
        display.Present(renderer);

        // Render Dear ImGui UI
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        RenderUI(sim, display);

        ImGui::Render();
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);

        SDL_RenderPresent(renderer);

        // --- Frame Timing ---
        FramePacing(sim, lastFrameTime, perfFreq);
    }

    // Cleanup
    sim.Shutdown();

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
```

## Frame Timing

The Atari runs at ~59.92 Hz (NTSC) or ~49.86 Hz (PAL). The main loop must
pace `Advance()` calls to match.

### Strategy: SDL3 Performance Counter

```cpp
void FramePacing(ATSimulator &sim, uint64 &lastFrameTime, uint64 perfFreq) {
    double targetFrameTime = sim.IsVideo50Hz()
        ? 1.0 / 49.8607  // PAL
        : 1.0 / 59.9227; // NTSC

    uint64 targetTicks = (uint64)(targetFrameTime * perfFreq);
    uint64 now = SDL_GetPerformanceCounter();
    uint64 elapsed = now - lastFrameTime;

    if (elapsed < targetTicks) {
        // Ahead of schedule -- wait
        uint64 remaining = targetTicks - elapsed;
        double remainingMs = (double)remaining / perfFreq * 1000.0;

        if (remainingMs > 1.0) {
            SDL_DelayPrecise((uint64)(remainingMs * 1000000.0)); // nanoseconds
        }

        lastFrameTime += targetTicks;  // accumulate to avoid drift
    } else {
        // Behind schedule -- don't wait, possibly drop frames
        lastFrameTime = now;
    }
}
```

`SDL_DelayPrecise()` provides nanosecond-precision delay using platform-
optimal mechanisms (spinning for sub-millisecond accuracy). This replaces
the `MsgWaitForMultipleObjects` + waitable timer approach used on Windows.

### VSync Alternative

If VSync is enabled (`SDL_SetRenderVSync(renderer, 1)`), `SDL_RenderPresent`
blocks until the next vertical blank. In this mode, frame timing is driven
by the display refresh rate. If the display is 60 Hz and the Atari is
~59.92 Hz, there will be occasional frame drops (~1 per 12 seconds) or
we can use adaptive timing that lets the emulation slightly adjust speed to
match the display.

For most users, VSync on a 60 Hz display provides the best experience.
`SDL_DelayPrecise` timing is the fallback for displays with non-standard
refresh rates.

## Key Differences from Windows Main Loop

| Aspect | Windows | SDL3 |
|--------|---------|------|
| Event dispatch | `TranslateMessage` / `DispatchMessage` | `SDL_PollEvent` |
| Idle processing | `WaitMessage` when nothing to do | Busy loop with frame pacing |
| Frame timing | `MsgWaitForMultipleObjects` + waitable timer | `SDL_DelayPrecise` or VSync |
| UI rendering | Win32 paints via `WM_PAINT` | Dear ImGui immediate-mode in main loop |
| Thread model | Single thread, message pump | Single thread, poll loop |

The fundamental model is the same: single-threaded, one frame of emulation
per iteration, display update after each frame. The difference is how we
wait for the next frame boundary.

## Interaction with ATSimulator

The SDL3 frontend needs to call these `ATSimulator` methods:

### Lifecycle
- `Init()` -- once at startup
- `Shutdown()` -- once at exit
- `ColdReset()` / `WarmReset()` -- on user request
- `Resume()` / `Suspend()` / `Pause()` -- run control

### Per-Frame
- `Advance(bool dropFrame)` -- run one frame of emulation
  - Returns `kAdvanceResult_Running` (still processing), `kAdvanceResult_Stopped`
    (emulation halted), or `kAdvanceResult_WaitingForFrame` (frame queue full,
    display must present before emulation can continue)
- `SetOnAdvanceUnblocked(vdfunction<void()>)` -- registers callback fired when
  a blocked `Advance` can retry (i.e., display consumed a queued frame)
- `GetGTIA().GetPresentedFrameCounter()` -- check if new frame ready

### Configuration
- `SetHardwareMode()`, `SetMemoryMode()`, `SetVideoStandard()`, etc.
- `GetFirmwareManager()`, `GetDeviceManager()` -- device/ROM configuration
- `Load()` -- load disk/cartridge/tape images

### Component Access
- `GetInputManager()` -- for routing input
- `GetGTIA()` -- for connecting display
- `GetAudioOutput()` -- for audio
- `GetDebugger()` -- for debugger (Phase 4)

## Frontend Project Structure

```
src/AltirraSDL/
    h/
        display_sdl3.h
        input_sdl3.h
        ui_state.h
    source/
        main_sdl3.cpp        -- Entry point, main loop, frame timing
        display_sdl3.cpp      -- VDVideoDisplaySDL3
        input_sdl3.cpp        -- SDL3 event → ATInputCode translation
        audioout_sdl3.cpp     -- (or in ATAudio, see AUDIO.md)
        ui_main.cpp           -- ImGui menu bar and orchestration
        ui_settings.cpp       -- Settings dialogs
        ui_devices.cpp        -- Device management dialogs
        ui_debugger.cpp       -- Debugger panes
        ui_overlay.cpp        -- Status overlay
        ui_filebrowser.cpp    -- File dialog integration
```

## Avoiding Global State

The Windows build uses globals (`g_sim`, `g_pDisplay`). The SDL3 frontend
can use locals in `main()` passed by reference/pointer to UI functions. This
improves testability and makes the code easier to reason about.

If global access is needed for compatibility with code shared between
frontends (e.g., the simulator sources), keep `g_sim` as a global but
define it in `main_sdl3.cpp` rather than sharing the Windows `main.cpp`.
