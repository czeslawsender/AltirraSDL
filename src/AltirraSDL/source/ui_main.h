//	AltirraSDL - Dear ImGui UI layer
//	Top-level UI state and rendering interface.

#pragma once

struct SDL_Window;
struct SDL_Renderer;
union SDL_Event;
class ATSimulator;
class VDVideoDisplaySDL3;

struct ATUIState {
	bool showDemoWindow = false;
	bool requestExit = false;

	// File dialog state (SDL3 async callback)
	bool fileDialogPending = false;
};

// Initialize Dear ImGui context and SDL3 backends.
bool ATUIInit(SDL_Window *window, SDL_Renderer *renderer);

// Shut down Dear ImGui.
void ATUIShutdown();

// Process an SDL event through ImGui. Returns true if ImGui consumed it.
bool ATUIProcessEvent(const SDL_Event *event);

// Returns true if ImGui wants keyboard input (a widget has focus).
bool ATUIWantCaptureKeyboard();

// Returns true if ImGui wants mouse input.
bool ATUIWantCaptureMouse();

// Render one frame of UI. Call between NewFrame and Render.
void ATUIRenderFrame(ATSimulator &sim, VDVideoDisplaySDL3 &display,
	SDL_Renderer *renderer, ATUIState &state);
