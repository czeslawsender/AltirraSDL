//	Altirra SDL3 frontend - main entry point (Phase 5: first pixels)

#include <stdafx.h>
#include <SDL3/SDL.h>
// Tell SDL3 we provide our own main() — don't rename it to SDL_main.
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

#include "simulator.h"
#include "gtia.h"
#include "firmwaremanager.h"

ATSimulator g_sim;
static VDVideoDisplaySDL3 *g_pDisplay = nullptr;
static SDL_Window   *g_pWindow   = nullptr;
static SDL_Renderer *g_pRenderer = nullptr;
static bool g_running = true;

static void HandleEvents() {
	SDL_Event ev;
	while (SDL_PollEvent(&ev)) {
		switch (ev.type) {
		case SDL_EVENT_QUIT:
		case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			g_running = false;
			break;
		case SDL_EVENT_KEY_DOWN:
			if (ev.key.key == SDLK_F12)
				g_sim.ColdReset();
			else if (ev.key.key == SDLK_F11)
				g_sim.WarmReset();
			else
				ATInputSDL3_HandleKeyDown(ev.key);
			break;
		case SDL_EVENT_KEY_UP:
			ATInputSDL3_HandleKeyUp(ev.key);
			break;
		}
	}
}

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

	// Enable vsync for natural frame rate limiting
	SDL_SetRenderVSync(g_pRenderer, 1);

	g_pDisplay = new VDVideoDisplaySDL3(g_pRenderer, 384*kScale, 240*kScale);

	g_sim.Init();

	// Load built-in kernel ROM — must happen before ColdReset
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

	// Main loop: advance emulator, present frames as they arrive
	while (g_running) {
		HandleEvents();
		if (!g_running) break;

		ATSimulator::AdvanceResult result = g_sim.Advance(false);

		switch (result) {
		case ATSimulator::kAdvanceResult_WaitingForFrame:
			// GTIA completed a frame and is waiting for us to consume it
			g_pDisplay->Present();
			break;

		case ATSimulator::kAdvanceResult_Running:
			// Still mid-frame — present if a frame is queued (keeps pipeline flowing)
			g_pDisplay->Present();
			break;

		case ATSimulator::kAdvanceResult_Stopped:
			g_pDisplay->Present();
			SDL_Delay(16);
			break;
		}
	}

	g_sim.GetGTIA().SetVideoOutput(nullptr);
	g_sim.Shutdown();

	delete g_pDisplay;
	SDL_DestroyRenderer(g_pRenderer);
	SDL_DestroyWindow(g_pWindow);
	SDL_Quit();
	return 0;
}
