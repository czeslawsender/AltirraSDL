//	Altirra SDL3 frontend - main entry point
//	Integrates SDL3 window, emulator core, and Dear ImGui UI.
//
//	Frame pacing:
//	  The Windows version uses a waitable timer + error accumulator to
//	  sleep between frames and hit the exact Atari frame rate (~59.92 Hz
//	  NTSC, ~49.86 Hz PAL).  We replicate this with SDL_DelayPrecise()
//	  and SDL_GetPerformanceCounter().  Vsync is still enabled but only
//	  as a secondary backstop; the primary rate limiter is our own timer.

#include <stdafx.h>
#include <SDL3/SDL.h>
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>
#include <stdio.h>

#include <vd2/system/vdtypes.h>
#include <vd2/system/VDString.h>
#include <vd2/system/text.h>
#include <vd2/system/registry.h>
#include <at/atcore/media.h>
#include <at/atio/image.h>

#include "display_sdl3_impl.h"
#include "input_sdl3.h"
#include "ui_main.h"

#include "simulator.h"
#include "gtia.h"
#include "firmwaremanager.h"

ATSimulator g_sim;
static VDVideoDisplaySDL3 *g_pDisplay = nullptr;
static SDL_Window   *g_pWindow   = nullptr;
static SDL_Renderer *g_pRenderer = nullptr;
static bool g_running = true;
static ATUIState g_uiState;

// =========================================================================
// Frame pacing — matches Windows main.cpp timing architecture
// =========================================================================

// Atari frame rates (from main.cpp ATUIUpdateSpeedTiming):
//   NTSC:  262 scanlines * 114 clocks @ 1.7897725 MHz = ~59.9227 Hz
//   PAL:   312 scanlines * 114 clocks @ 1.773447  MHz = ~49.8607 Hz
//   SECAM: 312 scanlines * 114 clocks @ 1.7815    MHz = ~50.0818 Hz
static constexpr double kFrameRate_NTSC  = 59.9227;
static constexpr double kFrameRate_PAL   = 49.8607;
static constexpr double kFrameRate_SECAM = 50.0818;

struct FramePacer {
	uint64_t perfFreq;          // SDL_GetPerformanceFrequency()
	uint64_t lastFrameTime;     // perf counter at last frame presentation
	double   targetSecsPerFrame;// seconds per emulated frame
	int64_t  errorAccum;        // timing error in perf counter ticks (positive = ahead)

	void Init() {
		perfFreq = SDL_GetPerformanceFrequency();
		lastFrameTime = SDL_GetPerformanceCounter();
		errorAccum = 0;
		UpdateRate(kFrameRate_NTSC);
	}

	void UpdateRate(double fps) {
		targetSecsPerFrame = 1.0 / fps;
	}

	// Called after a frame is complete.  Sleeps to maintain correct rate.
	void WaitForNextFrame() {
		uint64_t now = SDL_GetPerformanceCounter();
		int64_t elapsed = (int64_t)(now - lastFrameTime);
		int64_t targetTicks = (int64_t)(targetSecsPerFrame * (double)perfFreq);

		// Error accumulator: positive = we finished early, need to wait.
		// Mirrors the Windows "error += lastFrameDuration - g_frameTicks"
		// logic, but with inverted sign (they track lateness, we track
		// earliness).
		errorAccum += targetTicks - elapsed;

		// Clamp error to ±2 frames to prevent runaway drift (matches the
		// Windows g_frameErrorBound = 2 * g_frameTicks).
		int64_t errorBound = 2 * targetTicks;
		if (errorAccum > errorBound || errorAccum < -errorBound)
			errorAccum = 0;

		// If we're ahead of schedule, sleep.
		if (errorAccum > 0) {
			uint64_t waitNs = (uint64_t)((double)errorAccum / (double)perfFreq * 1e9);
			if (waitNs > 1000000) // only bother sleeping > 1ms
				SDL_DelayPrecise(waitNs);
		}

		lastFrameTime = SDL_GetPerformanceCounter();
	}
};

static FramePacer g_pacer;

// =========================================================================
// Event handling
// =========================================================================

static void HandleEvents() {
	SDL_Event ev;
	while (SDL_PollEvent(&ev)) {
		ATUIProcessEvent(&ev);

		switch (ev.type) {
		case SDL_EVENT_QUIT:
		case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			g_running = false;
			break;

		case SDL_EVENT_KEY_DOWN:
			if (!ATUIWantCaptureKeyboard()) {
				if (ev.key.key == SDLK_F12)
					g_sim.ColdReset();
				else if (ev.key.key == SDLK_F11)
					g_sim.WarmReset();
				else if (ev.key.key == SDLK_RETURN &&
					(ev.key.mod & SDL_KMOD_ALT)) {
					bool fs = (SDL_GetWindowFlags(g_pWindow) & SDL_WINDOW_FULLSCREEN) != 0;
					SDL_SetWindowFullscreen(g_pWindow, !fs);
				} else
					ATInputSDL3_HandleKeyDown(ev.key);
			}
			break;

		case SDL_EVENT_KEY_UP:
			if (!ATUIWantCaptureKeyboard())
				ATInputSDL3_HandleKeyUp(ev.key);
			break;

		default:
			break;
		}
	}
}

// =========================================================================
// Rendering
// =========================================================================

static void RenderAndPresent() {
	SDL_SetRenderDrawColor(g_pRenderer, 0, 0, 0, 255);
	SDL_RenderClear(g_pRenderer);

	// Draw emulator frame texture with blending disabled.
	// ImGui's SDLRenderer3 backend leaves SDL_BLENDMODE_BLEND active.
	SDL_Texture *emuTex = g_pDisplay->GetTexture();
	if (emuTex) {
		SDL_SetTextureBlendMode(emuTex, SDL_BLENDMODE_NONE);
		SDL_RenderTexture(g_pRenderer, emuTex, nullptr, nullptr);
	}

	// Draw ImGui UI on top
	ATUIRenderFrame(g_sim, *g_pDisplay, g_pRenderer, g_uiState);

	SDL_RenderPresent(g_pRenderer);
}

// =========================================================================
// Update frame pacer rate from current video standard
// =========================================================================

static void UpdatePacerRate() {
	switch (g_sim.GetVideoStandard()) {
	case kATVideoStandard_PAL:
	case kATVideoStandard_PAL60:
		g_pacer.UpdateRate(kFrameRate_PAL);
		break;
	case kATVideoStandard_SECAM:
		g_pacer.UpdateRate(kFrameRate_SECAM);
		break;
	default:
		g_pacer.UpdateRate(kFrameRate_NTSC);
		break;
	}
}

// =========================================================================
// Main
// =========================================================================

int main(int argc, char *argv[]) {
	fprintf(stderr, "[AltirraSDL] Starting...\n");

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) {
		fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return 1;
	}

	VDRegistryAppKey::setDefaultKey("AltirraSDL");

	const int kScale = 2;
	g_pWindow = SDL_CreateWindow("AltirraSDL", 384*kScale, 240*kScale, SDL_WINDOW_RESIZABLE);
	if (!g_pWindow) { fprintf(stderr, "CreateWindow: %s\n", SDL_GetError()); SDL_Quit(); return 1; }

	g_pRenderer = SDL_CreateRenderer(g_pWindow, nullptr);
	if (!g_pRenderer) { fprintf(stderr, "CreateRenderer: %s\n", SDL_GetError()); SDL_DestroyWindow(g_pWindow); SDL_Quit(); return 1; }

	SDL_SetRenderVSync(g_pRenderer, 1);

	if (!ATUIInit(g_pWindow, g_pRenderer)) {
		SDL_DestroyRenderer(g_pRenderer);
		SDL_DestroyWindow(g_pWindow);
		SDL_Quit();
		return 1;
	}

	g_pDisplay = new VDVideoDisplaySDL3(g_pRenderer, 384*kScale, 240*kScale);

	fprintf(stderr, "[AltirraSDL] Initializing simulator...\n");
	g_sim.Init();
	g_sim.LoadROMs();

	g_sim.GetGTIA().SetVideoOutput(g_pDisplay);
	ATInputSDL3_Init(&g_sim.GetPokey());

	if (argc > 1) {
		VDStringW widePath = VDTextU8ToW(argv[1], -1);
		ATImageLoadContext ctx {};
		if (!g_sim.Load(widePath.c_str(), kATMediaWriteMode_RO, &ctx))
			fprintf(stderr, "Warning: Could not load '%s'\n", argv[1]);
	}

	g_sim.ColdReset();
	g_sim.Resume();

	g_pacer.Init();
	UpdatePacerRate();

	// Present once immediately so compositors show the window.
	RenderAndPresent();

	fprintf(stderr, "[AltirraSDL] Entering main loop\n");

	// Main loop.  Mirrors the Windows idle handler structure:
	//
	// 1. Advance() runs emulation for up to g_ATSimScanlinesPerAdvance
	//    scanlines (default 32).  A full NTSC frame is 262 scanlines,
	//    so ~9 Advance() calls per frame.
	//
	// 2. When Advance() returns kAdvanceResult_WaitingForFrame, GTIA
	//    has completed a frame.  We upload it, present, and then SLEEP
	//    until it's time for the next frame (frame pacing).
	//
	// 3. The Windows version sleeps via MsgWaitForMultipleObjects or a
	//    waitable timer.  We use SDL_DelayPrecise() for equivalent
	//    nanosecond-precision sleep.
	//
	// Without this sleep, the emulator runs Advance() as fast as the
	// CPU allows, producing frames far faster than 60 Hz.

	while (g_running) {
		HandleEvents();
		if (!g_running) break;

		ATSimulator::AdvanceResult result = g_sim.Advance(false);

		// Check if a new frame arrived (GTIA called PostBuffer).
		// We must present whenever a frame is ready, regardless of
		// Advance() return value.  GTIA's PostBuffer and BeginFrame
		// can both happen inside a single Advance() call — the next
		// frame may already be in progress when Advance() returns
		// kAdvanceResult_Running.  The original main loop called
		// Present() on every Advance() result for this reason.
		bool hadFrame = g_pDisplay->IsFramePending();
		g_pDisplay->PrepareFrame();

		if (hadFrame) {
			// A frame was uploaded — present it and pace.
			RenderAndPresent();
			g_pacer.WaitForNextFrame();
		} else if (result == ATSimulator::kAdvanceResult_WaitingForFrame) {
			// GTIA is blocked but we had no frame to show.
			// Present anyway to keep UI responsive.
			RenderAndPresent();
			g_pacer.WaitForNextFrame();
		} else if (result == ATSimulator::kAdvanceResult_Stopped) {
			// Paused/stopped — render for UI, sleep to avoid busy-wait.
			RenderAndPresent();
			SDL_Delay(16);
		}
		// kAdvanceResult_Running with no frame: loop immediately.
	}

	g_sim.GetGTIA().SetVideoOutput(nullptr);
	g_sim.Shutdown();

	ATUIShutdown();

	delete g_pDisplay;
	SDL_DestroyRenderer(g_pRenderer);
	SDL_DestroyWindow(g_pWindow);
	SDL_Quit();
	return 0;
}
