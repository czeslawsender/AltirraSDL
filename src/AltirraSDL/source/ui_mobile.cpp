//	AltirraSDL - Mobile UI (hamburger menu, settings, file browser)
//	Touch-first UI for Android phones and tablets.
//	Provides hamburger slide-in menu, streamlined settings, and file browser.
//	All sizing uses density-independent pixels (dp) scaled by contentScale.

#include <stdafx.h>
#include <cwctype>
#include <vector>
#include <algorithm>
#include <SDL3/SDL.h>
#include <imgui.h>
#include <vd2/system/vdtypes.h>
#include <vd2/system/VDString.h>
#include <vd2/system/text.h>
#include <vd2/system/file.h>
#include <vd2/system/filesys.h>
#include <vd2/system/registry.h>
#include <at/atcore/media.h>

#include "ui_mobile.h"
#include "ui_main.h"
#include "touch_controls.h"
#include "simulator.h"
#include "gtia.h"
#include "mediamanager.h"
#include "firmwaremanager.h"
#include "uiaccessors.h"
#include "uitypes.h"
#include "constants.h"
#include <at/ataudio/audiooutput.h>

extern ATSimulator g_sim;
extern VDStringA ATGetConfigDir();

// Firmware scan function from ui_firmware.cpp
extern void ExecuteFirmwareScan(ATFirmwareManager *fwm, const VDStringW &scanDir);

// -------------------------------------------------------------------------
// dp helper — converts density-independent pixels to physical pixels
// -------------------------------------------------------------------------

static float s_contentScale = 1.0f;

static float dp(float v) { return v * s_contentScale; }

// -------------------------------------------------------------------------
// File browser state
// -------------------------------------------------------------------------

struct FileBrowserEntry {
	VDStringW name;
	VDStringW fullPath;
	bool isDirectory;
	bool operator<(const FileBrowserEntry &o) const {
		if (isDirectory != o.isDirectory) return isDirectory > o.isDirectory;
		return name < o.name;
	}
};

static std::vector<FileBrowserEntry> s_fileBrowserEntries;
static VDStringW s_fileBrowserDir;
static bool s_fileBrowserNeedsRefresh = true;

// ROM folder browser mode — when true, selecting a folder triggers firmware scan
static bool s_romFolderMode = false;
static VDStringW s_romDir;
static int s_romScanResult = -1;  // -1 = no scan yet, 0+ = number of ROMs found

// Supported file extensions for Atari images
static bool IsSupportedExtension(const wchar_t *name) {
	const wchar_t *ext = wcsrchr(name, L'.');
	if (!ext) return false;
	ext++;

	VDStringW extLower;
	for (const wchar_t *p = ext; *p; ++p)
		extLower += (wchar_t)towlower(*p);

	static const wchar_t *kExtensions[] = {
		L"xex", L"atr", L"car", L"bin", L"rom", L"cas",
		L"dcm", L"atz", L"zip", L"gz", L"xfd", L"atx",
		L"obx", L"com", L"exe",
		nullptr
	};

	for (const wchar_t **p = kExtensions; *p; ++p) {
		if (extLower == *p)
			return true;
	}
	return false;
}

static void RefreshFileBrowser(const VDStringW &dir) {
	s_fileBrowserEntries.clear();
	s_fileBrowserDir = dir;

	VDStringA dirU8 = VDTextWToU8(VDStringW(dir));

	struct EnumCtx {
		VDStringW baseDir;
		std::vector<FileBrowserEntry> *entries;
		bool romMode;
	};

	EnumCtx ctx;
	ctx.baseDir = dir;
	if (!ctx.baseDir.empty() && ctx.baseDir.back() != L'/')
		ctx.baseDir += L'/';
	ctx.entries = &s_fileBrowserEntries;
	ctx.romMode = s_romFolderMode;

	auto callback = [](void *userdata, const char *dirname, const char *fname) -> SDL_EnumerationResult {
		EnumCtx *ctx = (EnumCtx *)userdata;

		if (fname[0] == '.')
			return SDL_ENUM_CONTINUE;

		VDStringW wname = VDTextU8ToW(VDStringA(fname));
		VDStringW fullPath = ctx->baseDir + wname;
		VDStringA fullPathU8 = VDTextWToU8(fullPath);

		FileBrowserEntry entry;
		entry.name = std::move(wname);
		entry.fullPath = std::move(fullPath);

		SDL_PathInfo info;
		if (SDL_GetPathInfo(fullPathU8.c_str(), &info)) {
			entry.isDirectory = (info.type == SDL_PATHTYPE_DIRECTORY);
		} else {
			entry.isDirectory = false;
		}

		// In ROM folder mode, only show directories
		if (ctx->romMode) {
			if (entry.isDirectory)
				ctx->entries->push_back(std::move(entry));
		} else {
			if (entry.isDirectory || IsSupportedExtension(entry.name.c_str()))
				ctx->entries->push_back(std::move(entry));
		}

		return SDL_ENUM_CONTINUE;
	};

	SDL_EnumerateDirectory(dirU8.c_str(), callback, &ctx);
	std::sort(s_fileBrowserEntries.begin(), s_fileBrowserEntries.end());
	s_fileBrowserNeedsRefresh = false;
}

// -------------------------------------------------------------------------
// Init
// -------------------------------------------------------------------------

void ATMobileUI_Init() {
#ifdef __ANDROID__
	// SDL_GetUserFolder returns NULL on Android because scoped storage
	// requires explicit MANAGE_EXTERNAL_STORAGE / SAF permissions we do
	// not have. Fall back to the public Downloads directory (readable
	// with READ_EXTERNAL_STORAGE on API<=32, via MediaStore on newer
	// versions) and finally to the app's private external files dir,
	// which is always writable without any permission.
	const char *dl = SDL_GetUserFolder(SDL_FOLDER_DOWNLOADS);
	if (dl && *dl) {
		s_fileBrowserDir = VDTextU8ToW(VDStringA(dl));
	} else {
		const char *ext = SDL_GetAndroidExternalStoragePath();
		if (ext && *ext)
			s_fileBrowserDir = VDTextU8ToW(VDStringA(ext));
		else
			s_fileBrowserDir = VDTextU8ToW(ATGetConfigDir());
	}
#else
	const char *home = SDL_GetUserFolder(SDL_FOLDER_HOME);
	if (home)
		s_fileBrowserDir = VDTextU8ToW(VDStringA(home));
	else
		s_fileBrowserDir = L"/";
#endif
	s_fileBrowserNeedsRefresh = true;
}

bool ATMobileUI_IsFirstRun() {
	return false;
}

// -------------------------------------------------------------------------
// Menu button polling
// -------------------------------------------------------------------------

extern bool s_menuTapped;

static bool ConsumeMenuTap() {
	if (s_menuTapped) {
		s_menuTapped = false;
		return true;
	}
	return false;
}

// -------------------------------------------------------------------------
// Hamburger menu
// -------------------------------------------------------------------------

static bool s_wasPausedBeforeMenu = false;

void ATMobileUI_OpenMenu(ATSimulator &sim, ATMobileUIState &mobileState) {
	s_wasPausedBeforeMenu = sim.IsPaused();
	sim.Pause();
	ATTouchControls_ReleaseAll();
	mobileState.currentScreen = ATMobileUIScreen::HamburgerMenu;
}

void ATMobileUI_CloseMenu(ATSimulator &sim, ATMobileUIState &mobileState) {
	mobileState.currentScreen = ATMobileUIScreen::None;
	if (!s_wasPausedBeforeMenu)
		sim.Resume();
}

static void RenderHamburgerMenu(ATSimulator &sim, ATUIState &uiState,
	ATMobileUIState &mobileState, SDL_Window *window)
{
	ImGuiIO &io = ImGui::GetIO();
	float menuW = io.DisplaySize.x * 0.65f;
	float minW = dp(280.0f);
	float maxW = dp(400.0f);
	if (menuW < minW) menuW = minW;
	if (menuW > maxW) menuW = maxW;

	// Dim background
	ImGui::GetBackgroundDrawList()->AddRectFilled(
		ImVec2(0, 0), io.DisplaySize,
		IM_COL32(0, 0, 0, 128));

	// Menu panel (slides from right)
	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - menuW, 0));
	ImGui::SetNextWindowSize(ImVec2(menuW, io.DisplaySize.y));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

	if (ImGui::Begin("##MobileMenu", nullptr, flags)) {
		// Title bar with close button
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + dp(8.0f));
		ImGui::Text("Altirra");
		ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - dp(32.0f));
		if (ImGui::Button("X", ImVec2(dp(32.0f), dp(32.0f))))
			ATMobileUI_CloseMenu(sim, mobileState);

		ImGui::Separator();
		ImGui::Spacing();

		// Menu button height scaled for touch
		float btnH = dp(56.0f);
		ImVec2 btnSize(-1, btnH);

		// Resume
		if (ImGui::Button("Resume", btnSize))
			ATMobileUI_CloseMenu(sim, mobileState);
		ImGui::Spacing();

		// Load Game
		if (ImGui::Button("Load Game", btnSize)) {
			s_romFolderMode = false;
			mobileState.currentScreen = ATMobileUIScreen::FileBrowser;
			s_fileBrowserNeedsRefresh = true;
		}
		ImGui::Spacing();

		// Disk Drives
		if (ImGui::Button("Disk Drives", btnSize)) {
			ATMobileUI_CloseMenu(sim, mobileState);
			uiState.showDiskManager = true;
		}
		ImGui::Spacing();

		// Audio toggle
		{
			const char *audioLabel = mobileState.audioMuted ? "Audio: OFF" : "Audio: ON";
			if (ImGui::Button(audioLabel, btnSize)) {
				mobileState.audioMuted = !mobileState.audioMuted;
				IATAudioOutput *audioOut = g_sim.GetAudioOutput();
				if (audioOut)
					audioOut->SetMute(mobileState.audioMuted);
			}
		}
		ImGui::Spacing();

		ImGui::Separator();
		ImGui::Spacing();

		// Warm Reset
		if (ImGui::Button("Warm Reset", btnSize)) {
			sim.WarmReset();
			ATMobileUI_CloseMenu(sim, mobileState);
			sim.Resume();
		}
		ImGui::Spacing();

		// Cold Reset
		if (ImGui::Button("Cold Reset", btnSize)) {
			sim.ColdReset();
			ATMobileUI_CloseMenu(sim, mobileState);
			sim.Resume();
		}
		ImGui::Spacing();

		ImGui::Separator();
		ImGui::Spacing();

		// Virtual Keyboard (Phase 2 placeholder — disabled)
		ImGui::BeginDisabled(true);
		ImGui::Button("Virtual Keyboard", btnSize);
		ImGui::EndDisabled();
		ImGui::Spacing();

		// Settings
		if (ImGui::Button("Settings", btnSize))
			mobileState.currentScreen = ATMobileUIScreen::Settings;
		ImGui::Spacing();

		ImGui::Separator();
		ImGui::Spacing();

		// About
		if (ImGui::Button("About", btnSize)) {
			ATMobileUI_CloseMenu(sim, mobileState);
			uiState.showAboutDialog = true;
		}
	}
	ImGui::End();

	// Tap outside menu to close
	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		ImVec2 mousePos = ImGui::GetMousePos();
		if (mousePos.x < io.DisplaySize.x - menuW)
			ATMobileUI_CloseMenu(sim, mobileState);
	}
}

// -------------------------------------------------------------------------
// File browser
// -------------------------------------------------------------------------

static void NavigateUp() {
	VDStringW parent = s_fileBrowserDir;
	while (!parent.empty() && parent.back() == L'/')
		parent.pop_back();
	for (size_t i = parent.size(); i > 0; --i) {
		if (parent[i - 1] == L'/') {
			if (i > 1)
				parent.resize(i - 1);
			else
				parent.resize(1);  // keep root "/"
			s_fileBrowserDir = parent;
			s_fileBrowserNeedsRefresh = true;
			return;
		}
	}
}

static void RenderFileBrowser(ATSimulator &sim, ATUIState &uiState,
	ATMobileUIState &mobileState, SDL_Window *window)
{
	if (s_fileBrowserNeedsRefresh)
		RefreshFileBrowser(s_fileBrowserDir);

	ImGuiIO &io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(io.DisplaySize);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

	if (ImGui::Begin("##FileBrowser", nullptr, flags)) {
		// Header bar
		float headerH = dp(48.0f);
		ImVec2 backBtnSize(dp(48.0f), headerH);

		if (ImGui::Button("<", backBtnSize)) {
			if (s_romFolderMode) {
				s_romFolderMode = false;
				mobileState.currentScreen = ATMobileUIScreen::Settings;
			} else {
				ATMobileUI_CloseMenu(sim, mobileState);
			}
		}
		ImGui::SameLine();

		const char *title = s_romFolderMode ? "Select ROM Folder" : "Load Game";
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (headerH - ImGui::GetTextLineHeight()) * 0.5f);
		ImGui::Text("%s", title);

		ImGui::Separator();

		// Current directory
		VDStringA dirU8 = VDTextWToU8(s_fileBrowserDir);
		ImGui::TextWrapped("%s", dirU8.c_str());

		// Navigation row: Up + (in ROM mode) "Select This Folder" button
		float rowBtnH = dp(48.0f);
		if (ImGui::Button(".. (Up)", ImVec2(dp(120.0f), rowBtnH)))
			NavigateUp();

		if (s_romFolderMode) {
			ImGui::SameLine();
			if (ImGui::Button("Use This Folder", ImVec2(-1, rowBtnH))) {
				// Trigger firmware scan on current directory
				s_romDir = s_fileBrowserDir;
				ATFirmwareManager *fwm = g_sim.GetFirmwareManager();
				ExecuteFirmwareScan(fwm, s_romDir);

				// Count detected firmware
				vdvector<ATFirmwareInfo> fwList;
				fwm->GetFirmwareList(fwList);
				s_romScanResult = (int)fwList.size();

				// Reload ROMs after scan so new firmware is active
				g_sim.LoadROMs();

				// Return to settings
				s_romFolderMode = false;
				mobileState.currentScreen = ATMobileUIScreen::Settings;
			}
		}

		ImGui::Separator();

		// File/directory list
		float itemH = dp(56.0f);
		ImGui::BeginChild("FileList", ImVec2(0, 0), ImGuiChildFlags_None);

		for (size_t i = 0; i < s_fileBrowserEntries.size(); i++) {
			const FileBrowserEntry &entry = s_fileBrowserEntries[i];
			VDStringA nameU8 = VDTextWToU8(VDStringW(entry.name));

			char label[512];
			if (entry.isDirectory)
				snprintf(label, sizeof(label), "[DIR] %s", nameU8.c_str());
			else
				snprintf(label, sizeof(label), "      %s", nameU8.c_str());

			ImGui::PushID((int)i);
			if (ImGui::Selectable(label, (int)i == mobileState.selectedFileIdx,
				0, ImVec2(0, itemH)))
			{
				if (entry.isDirectory) {
					s_fileBrowserDir = entry.fullPath;
					s_fileBrowserNeedsRefresh = true;
					mobileState.selectedFileIdx = -1;
				} else if (!s_romFolderMode) {
					mobileState.selectedFileIdx = (int)i;
					VDStringA pathU8 = VDTextWToU8(VDStringW(entry.fullPath));
					ATUIPushDeferred(kATDeferred_BootImage, pathU8.c_str());
					mobileState.gameLoaded = true;
					ATMobileUI_CloseMenu(sim, mobileState);
					sim.Resume();
				}
			}
			ImGui::PopID();
		}

		ImGui::EndChild();
	}
	ImGui::End();
}

// -------------------------------------------------------------------------
// Settings
// -------------------------------------------------------------------------

static void RenderSettings(ATSimulator &sim, ATUIState &uiState,
	ATMobileUIState &mobileState, SDL_Window *window)
{
	ImGuiIO &io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(io.DisplaySize);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

	if (ImGui::Begin("##MobileSettings", nullptr, flags)) {
		// Header
		float headerH = dp(48.0f);
		if (ImGui::Button("<", ImVec2(dp(48.0f), headerH)))
			mobileState.currentScreen = ATMobileUIScreen::HamburgerMenu;
		ImGui::SameLine();
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (headerH - ImGui::GetTextLineHeight()) * 0.5f);
		ImGui::Text("Settings");

		ImGui::Separator();
		ImGui::Spacing();

		ImGui::BeginChild("SettingsScroll", ImVec2(0, 0), ImGuiChildFlags_None);

		// ---- SYSTEM ----
		ImGui::SeparatorText("System");

		// Video Standard — PAL / NTSC
		{
			int current = (sim.GetVideoStandard() == kATVideoStandard_PAL) ? 0 : 1;
			const char *items[] = { "PAL", "NTSC" };
			ImGui::SetNextItemWidth(-1);
			if (ImGui::Combo("Video Standard", &current, items, 2))
				sim.SetVideoStandard(current == 0 ? kATVideoStandard_PAL : kATVideoStandard_NTSC);
		}

		// Memory Size
		{
			static const struct {
				const char *label;
				ATMemoryMode mode;
			} kMemModes[] = {
				{ "16K", kATMemoryMode_16K },
				{ "48K", kATMemoryMode_48K },
				{ "64K", kATMemoryMode_64K },
				{ "128K", kATMemoryMode_128K },
				{ "320K (Compy Shop)", kATMemoryMode_320K },
				{ "1088K", kATMemoryMode_1088K },
			};

			ATMemoryMode curMode = sim.GetMemoryMode();
			int curIdx = 4; // default 320K
			int count = (int)(sizeof(kMemModes)/sizeof(kMemModes[0]));
			for (int i = 0; i < count; i++) {
				if (kMemModes[i].mode == curMode) { curIdx = i; break; }
			}

			const char *labels[16];
			for (int i = 0; i < count; i++) labels[i] = kMemModes[i].label;

			ImGui::SetNextItemWidth(-1);
			if (ImGui::Combo("Memory Size", &curIdx, labels, count))
				sim.SetMemoryMode(kMemModes[curIdx].mode);
		}

		// BASIC toggle
		{
			bool basicEnabled = sim.IsBASICEnabled();
			if (ImGui::Checkbox("BASIC Enabled", &basicEnabled))
				sim.SetBASICEnabled(basicEnabled);
		}

		// SIO Patch toggle
		{
			bool sioEnabled = sim.IsSIOPatchEnabled();
			if (ImGui::Checkbox("SIO Patch", &sioEnabled))
				sim.SetSIOPatchEnabled(sioEnabled);
		}

		ImGui::Spacing();

		// ---- CONTROLS ----
		ImGui::SeparatorText("Controls");

		// Control size
		{
			int sz = (int)mobileState.layoutConfig.controlSize;
			const char *sizes[] = { "Small", "Medium", "Large" };
			ImGui::SetNextItemWidth(-1);
			if (ImGui::Combo("Control Size", &sz, sizes, 3))
				mobileState.layoutConfig.controlSize = (ATTouchControlSize)sz;
		}

		// Control opacity — fix format to show 10%-100%
		{
			int pct = (int)(mobileState.layoutConfig.controlOpacity * 100.0f + 0.5f);
			if (ImGui::SliderInt("Opacity", &pct, 10, 100, "%d%%"))
				mobileState.layoutConfig.controlOpacity = pct / 100.0f;
		}

		// Haptic feedback
		ImGui::Checkbox("Haptic Feedback", &mobileState.layoutConfig.hapticEnabled);

		ImGui::Spacing();

		// ---- DISPLAY ----
		ImGui::SeparatorText("Display");

		// Filter mode
		{
			ATDisplayFilterMode curFM = ATUIGetDisplayFilterMode();
			int idx = 0;
			switch (curFM) {
			case kATDisplayFilterMode_Point:        idx = 0; break;
			case kATDisplayFilterMode_Bilinear:     idx = 1; break;
			case kATDisplayFilterMode_SharpBilinear:idx = 2; break;
			default: idx = 1; break;
			}
			const char *filters[] = { "Sharp (Nearest)", "Bilinear", "Sharp Bilinear" };
			ImGui::SetNextItemWidth(-1);
			if (ImGui::Combo("Filter Mode", &idx, filters, 3)) {
				static const ATDisplayFilterMode kModes[] = {
					kATDisplayFilterMode_Point,
					kATDisplayFilterMode_Bilinear,
					kATDisplayFilterMode_SharpBilinear,
				};
				ATUISetDisplayFilterMode(kModes[idx]);
			}
		}

		ImGui::Spacing();

		// ---- FIRMWARE ----
		ImGui::SeparatorText("Firmware");

		{
			if (!s_romDir.empty()) {
				VDStringA dirU8 = VDTextWToU8(s_romDir);
				ImGui::TextWrapped("ROM Directory: %s", dirU8.c_str());
			} else {
				ImGui::Text("ROM Directory: (not set)");
			}

			if (s_romScanResult >= 0)
				ImGui::Text("Status: %d ROMs found", s_romScanResult);

			if (ImGui::Button("Select ROM Folder", ImVec2(-1, dp(48.0f)))) {
				// Switch to file browser in ROM folder selection mode
				s_romFolderMode = true;
				s_fileBrowserNeedsRefresh = true;
				mobileState.currentScreen = ATMobileUIScreen::FileBrowser;
			}
		}

		ImGui::EndChild();
	}
	ImGui::End();
}

// -------------------------------------------------------------------------
// Main render entry point
// -------------------------------------------------------------------------

void ATMobileUI_Render(ATSimulator &sim, ATUIState &uiState,
	ATMobileUIState &mobileState, SDL_Window *window)
{
	// Cache content scale for dp() helper
	s_contentScale = mobileState.layoutConfig.contentScale;

	int w, h;
	SDL_GetWindowSize(window, &w, &h);

	// Update layout if screen size or config changed
	if (w != mobileState.layout.screenW || h != mobileState.layout.screenH
		|| mobileState.layoutConfig.controlSize != mobileState.layout.lastControlSize
		|| mobileState.layoutConfig.contentScale != mobileState.layout.lastContentScale)
	{
		ATTouchLayout_Update(mobileState.layout, w, h, mobileState.layoutConfig);
	}

	// Check for menu button tap
	if (ConsumeMenuTap() && mobileState.currentScreen == ATMobileUIScreen::None)
		ATMobileUI_OpenMenu(sim, mobileState);

	switch (mobileState.currentScreen) {
	case ATMobileUIScreen::None:
		// Render touch controls overlay
		ATTouchControls_Render(mobileState.layout, mobileState.layoutConfig);

		// If no game loaded, show centered "Load Game" button
		if (!mobileState.gameLoaded) {
			ImGuiIO &io = ImGui::GetIO();
			ImVec2 center = io.DisplaySize;
			center.x *= 0.5f;
			center.y *= 0.5f;
			ImVec2 btnSize(dp(200.0f), dp(56.0f));
			ImGui::SetNextWindowPos(center, 0, ImVec2(0.5f, 0.5f));
			ImGui::SetNextWindowSize(ImVec2(btnSize.x + dp(40.0f), btnSize.y + dp(40.0f)));
			ImGui::Begin("##LoadPrompt", nullptr,
				ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
				| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
				| ImGuiWindowFlags_NoBackground);
			if (ImGui::Button("Load Game", btnSize)) {
				ATMobileUI_OpenMenu(sim, mobileState);
				s_romFolderMode = false;
				mobileState.currentScreen = ATMobileUIScreen::FileBrowser;
				s_fileBrowserNeedsRefresh = true;
			}
			ImGui::End();
		}
		break;

	case ATMobileUIScreen::HamburgerMenu:
		RenderHamburgerMenu(sim, uiState, mobileState, window);
		break;

	case ATMobileUIScreen::FileBrowser:
		RenderFileBrowser(sim, uiState, mobileState, window);
		break;

	case ATMobileUIScreen::Settings:
		RenderSettings(sim, uiState, mobileState, window);
		break;

	case ATMobileUIScreen::FirstRunWizard:
		// TODO: Phase 2 — first-boot firmware wizard
		break;
	}
}

// -------------------------------------------------------------------------
// Event handling
// -------------------------------------------------------------------------

bool ATMobileUI_HandleEvent(const SDL_Event &ev, ATMobileUIState &mobileState) {
	// If a menu/dialog is open, let ImGui handle everything
	if (mobileState.currentScreen != ATMobileUIScreen::None)
		return false;

	// Route touch events to touch controls
	if (ev.type == SDL_EVENT_FINGER_DOWN ||
		ev.type == SDL_EVENT_FINGER_MOTION ||
		ev.type == SDL_EVENT_FINGER_UP)
	{
		return ATTouchControls_HandleEvent(ev, mobileState.layout);
	}

	return false;
}

void ATMobileUI_OpenFileBrowser(ATMobileUIState &mobileState) {
	s_romFolderMode = false;
	mobileState.currentScreen = ATMobileUIScreen::FileBrowser;
	s_fileBrowserNeedsRefresh = true;
}

void ATMobileUI_OpenSettings(ATMobileUIState &mobileState) {
	mobileState.currentScreen = ATMobileUIScreen::Settings;
}
