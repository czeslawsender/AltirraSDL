//	AltirraSDL - Dear ImGui UI layer
//	Implements menu bar, status overlay, and file dialog integration.
//	Menu structure mirrors Altirra's menu_default.txt for cross-platform
//	consistency (see PORTING/UI.md).

#include <stdafx.h>
#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <vd2/system/vdtypes.h>
#include <vd2/system/VDString.h>
#include <vd2/system/text.h>
#include <at/atcore/media.h>
#include <at/atio/image.h>

#include "ui_main.h"
#include "display_sdl3_impl.h"
#include "simulator.h"
#include "gtia.h"

extern ATSimulator g_sim;

// =========================================================================
// File dialog callback (SDL3 async)
// =========================================================================

static void FileDialogCallback(void *userdata, const char * const *filelist, int filter) {
	if (!filelist || !filelist[0])
		return;

	VDStringW widePath = VDTextU8ToW(filelist[0], -1);
	ATImageLoadContext ctx {};
	if (!g_sim.Load(widePath.c_str(), kATMediaWriteMode_RO, &ctx))
		fprintf(stderr, "[AltirraSDL] Warning: could not load '%s'\n", filelist[0]);
	else {
		g_sim.ColdReset();
		fprintf(stderr, "[AltirraSDL] Loaded: %s\n", filelist[0]);
	}
}

// =========================================================================
// Init / Shutdown
// =========================================================================

bool ATUIInit(SDL_Window *window, SDL_Renderer *renderer) {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.IniFilename = "altirrasdl_imgui.ini";

	ImGui::StyleColorsDark();

	// Slightly adjust style for emulator aesthetic
	ImGuiStyle& style = ImGui::GetStyle();
	style.FrameRounding = 2.0f;
	style.WindowRounding = 4.0f;
	style.GrabRounding = 2.0f;

	if (!ImGui_ImplSDL3_InitForSDLRenderer(window, renderer)) {
		fprintf(stderr, "[AltirraSDL] ImGui SDL3 init failed\n");
		return false;
	}

	if (!ImGui_ImplSDLRenderer3_Init(renderer)) {
		fprintf(stderr, "[AltirraSDL] ImGui SDLRenderer3 init failed\n");
		return false;
	}

	fprintf(stderr, "[AltirraSDL] ImGui initialized (docking enabled)\n");
	return true;
}

void ATUIShutdown() {
	ImGui_ImplSDLRenderer3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
}

bool ATUIProcessEvent(const SDL_Event *event) {
	return ImGui_ImplSDL3_ProcessEvent(event);
}

bool ATUIWantCaptureKeyboard() {
	return ImGui::GetIO().WantCaptureKeyboard;
}

bool ATUIWantCaptureMouse() {
	return ImGui::GetIO().WantCaptureMouse;
}

// =========================================================================
// Menu bar — mirrors menu_default.txt structure
// =========================================================================

static void RenderFileMenu(ATSimulator &sim, SDL_Window *window) {
	if (ImGui::MenuItem("Boot Image...", "Ctrl+O")) {
		static const SDL_DialogFileFilter filters[] = {
			{ "Atari Images", "atr;xfd;dcm;xex;obx;com;bin;rom;car;cas;wav;gz;zip;atz" },
			{ "All Files", "*" },
		};
		SDL_ShowOpenFileDialog(FileDialogCallback, nullptr, window,
			filters, 2, nullptr, false);
	}

	ImGui::Separator();

	if (ImGui::MenuItem("Exit")) {
		SDL_Event quit{};
		quit.type = SDL_EVENT_QUIT;
		SDL_PushEvent(&quit);
	}
}

static void RenderViewMenu(ATSimulator &sim, SDL_Window *window) {
	bool isFullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
	if (ImGui::MenuItem("Full Screen", "Alt+Enter", isFullscreen))
		SDL_SetWindowFullscreen(window, !isFullscreen);

	ImGui::Separator();

	// Filter mode (display only — actual implementation in Phase 6+)
	if (ImGui::BeginMenu("Filter Mode")) {
		ImGui::MenuItem("Point", nullptr, false, false);
		ImGui::MenuItem("Bilinear", nullptr, false, false);
		ImGui::MenuItem("Sharp Bilinear", nullptr, false, false);
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Overscan Mode")) {
		ImGui::MenuItem("OS Screen Only", nullptr, false, false);
		ImGui::MenuItem("Normal", nullptr, false, false);
		ImGui::MenuItem("Extended", nullptr, false, false);
		ImGui::MenuItem("Full", nullptr, false, false);
		ImGui::EndMenu();
	}
}

static void RenderSystemMenu(ATSimulator &sim) {
	if (ImGui::MenuItem("Warm Reset", "F5"))
		sim.WarmReset();

	if (ImGui::MenuItem("Cold Reset", "Shift+F5"))
		sim.ColdReset();

	ImGui::Separator();

	bool paused = sim.IsPaused();
	if (ImGui::MenuItem("Pause", "F9", paused)) {
		if (paused)
			sim.Resume();
		else
			sim.Pause();
	}

	ImGui::Separator();

	if (ImGui::BeginMenu("Video Standard")) {
		ATVideoStandard vs = sim.GetVideoStandard();
		if (ImGui::MenuItem("NTSC", nullptr,
				vs == kATVideoStandard_NTSC))
			sim.SetVideoStandard(kATVideoStandard_NTSC);
		if (ImGui::MenuItem("PAL", nullptr,
				vs == kATVideoStandard_PAL))
			sim.SetVideoStandard(kATVideoStandard_PAL);
		if (ImGui::MenuItem("SECAM", nullptr,
				vs == kATVideoStandard_SECAM))
			sim.SetVideoStandard(kATVideoStandard_SECAM);
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Hardware Mode")) {
		ATHardwareMode hw = sim.GetHardwareMode();
		if (ImGui::MenuItem("800", nullptr,
				hw == kATHardwareMode_800))
			sim.SetHardwareMode(kATHardwareMode_800);
		if (ImGui::MenuItem("800XL", nullptr,
				hw == kATHardwareMode_800XL))
			sim.SetHardwareMode(kATHardwareMode_800XL);
		if (ImGui::MenuItem("130XE", nullptr,
				hw == kATHardwareMode_130XE))
			sim.SetHardwareMode(kATHardwareMode_130XE);
		if (ImGui::MenuItem("1200XL", nullptr,
				hw == kATHardwareMode_1200XL))
			sim.SetHardwareMode(kATHardwareMode_1200XL);
		if (ImGui::MenuItem("XEGS", nullptr,
				hw == kATHardwareMode_XEGS))
			sim.SetHardwareMode(kATHardwareMode_XEGS);
		if (ImGui::MenuItem("5200", nullptr,
				hw == kATHardwareMode_5200))
			sim.SetHardwareMode(kATHardwareMode_5200);
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Memory")) {
		ATMemoryMode mm = sim.GetMemoryMode();
		if (ImGui::MenuItem("48K", nullptr,
				mm == kATMemoryMode_48K))
			sim.SetMemoryMode(kATMemoryMode_48K);
		if (ImGui::MenuItem("64K", nullptr,
				mm == kATMemoryMode_64K))
			sim.SetMemoryMode(kATMemoryMode_64K);
		if (ImGui::MenuItem("128K", nullptr,
				mm == kATMemoryMode_128K))
			sim.SetMemoryMode(kATMemoryMode_128K);
		if (ImGui::MenuItem("320K (Compy Shop)", nullptr,
				mm == kATMemoryMode_320K))
			sim.SetMemoryMode(kATMemoryMode_320K);
		if (ImGui::MenuItem("576K (Compy Shop)", nullptr,
				mm == kATMemoryMode_576K))
			sim.SetMemoryMode(kATMemoryMode_576K);
		if (ImGui::MenuItem("1088K", nullptr,
				mm == kATMemoryMode_1088K))
			sim.SetMemoryMode(kATMemoryMode_1088K);
		ImGui::EndMenu();
	}
}

static void RenderInputMenu() {
	ImGui::MenuItem("Input Mappings...", nullptr, false, false);
	ImGui::Separator();
	ImGui::MenuItem("Capture Mouse", nullptr, false, false);
}

static void RenderDebugMenu(ATSimulator &sim) {
	ImGui::MenuItem("Enable Debugger", nullptr, false, false);
	ImGui::Separator();
	if (ImGui::BeginMenu("Window")) {
		ImGui::MenuItem("Console", nullptr, false, false);
		ImGui::MenuItem("Registers", nullptr, false, false);
		ImGui::MenuItem("Disassembly", nullptr, false, false);
		ImGui::MenuItem("Memory", nullptr, false, false);
		ImGui::EndMenu();
	}
}

static void RenderToolsMenu() {
	ImGui::MenuItem("Disk Explorer...", nullptr, false, false);
	ImGui::Separator();
	ImGui::MenuItem("Keyboard Shortcuts...", nullptr, false, false);
}

static void RenderHelpMenu() {
	if (ImGui::MenuItem("About"))
		; // TODO: about dialog
}

static void RenderMainMenu(ATSimulator &sim, SDL_Window *window) {
	if (!ImGui::BeginMainMenuBar())
		return;

	if (ImGui::BeginMenu("File")) {
		RenderFileMenu(sim, window);
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("View")) {
		RenderViewMenu(sim, window);
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("System")) {
		RenderSystemMenu(sim);
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Input")) {
		RenderInputMenu();
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Debug")) {
		RenderDebugMenu(sim);
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Tools")) {
		RenderToolsMenu();
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Help")) {
		RenderHelpMenu();
		ImGui::EndMenu();
	}

	ImGui::EndMainMenuBar();
}

// =========================================================================
// Status overlay — FPS + emulation state
// =========================================================================

static void RenderStatusOverlay(ATSimulator &sim) {
	const ImGuiIO& io = ImGui::GetIO();

	ImGui::SetNextWindowPos(
		ImVec2(io.DisplaySize.x - 10.0f, io.DisplaySize.y - 10.0f),
		ImGuiCond_Always,
		ImVec2(1.0f, 1.0f));
	ImGui::SetNextWindowBgAlpha(0.5f);

	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoInputs;

	if (ImGui::Begin("##StatusOverlay", nullptr, flags)) {
		ImGui::Text("%.0f FPS", io.Framerate);

		if (sim.IsPaused()) {
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "PAUSED");
		}
	}
	ImGui::End();
}

// =========================================================================
// Top-level frame render
// =========================================================================

void ATUIRenderFrame(ATSimulator &sim, VDVideoDisplaySDL3 &display,
	SDL_Renderer *renderer, ATUIState &state)
{
	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	// Menu bar
	RenderMainMenu(sim, SDL_GetRenderWindow(renderer));

	// Status overlay
	RenderStatusOverlay(sim);

	// Demo window for development/testing
	if (state.showDemoWindow)
		ImGui::ShowDemoWindow(&state.showDemoWindow);

	ImGui::Render();
	ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
}
