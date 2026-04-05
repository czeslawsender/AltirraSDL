//	AltirraSDL - Virtual touch controls for Android
//	Handles multi-touch finger tracking, joystick/dpad computation,
//	button hit testing, and ImGui overlay rendering.
//	Routes input through ATInputManager (joystick/fire) and GTIA
//	(console switches) — same paths as physical gamepads.

#include <stdafx.h>
#include <cmath>
#include <imgui.h>
#include <SDL3/SDL.h>
#include "touch_controls.h"
#include "inputmanager.h"
#include "inputdefs.h"
#include "gtia.h"

// -------------------------------------------------------------------------
// State
// -------------------------------------------------------------------------

static ATInputManager *s_pInputManager = nullptr;
static ATGTIAEmulator *s_pGTIA = nullptr;

// Joystick finger tracking
static SDL_FingerID s_joyFinger = 0;
static bool s_joyActive = false;
static float s_joyBaseX = 0, s_joyBaseY = 0;  // Touch-down position (pixels)
static float s_joyCurX = 0, s_joyCurY = 0;    // Current position (pixels)
static uint8 s_joyDirMask = 0;                // Active directions (bit 0=L,1=R,2=U,3=D)

// Fire button finger tracking
static SDL_FingerID s_fireFinger = 0;
static bool s_fireActive = false;
static bool s_fireAHeld = false;
static bool s_fireBHeld = false;

// Console button tracking (START/SELECT/OPTION each track independently)
static SDL_FingerID s_consoleFinger = 0;
static bool s_consoleActive = false;
static bool s_startHeld = false;
static bool s_selectHeld = false;
static bool s_optionHeld = false;

// Menu button tap detection — not static, accessed by ui_mobile.cpp
bool s_menuTapped = false;

// -------------------------------------------------------------------------
// Direction conversion
// -------------------------------------------------------------------------

// Convert joystick displacement to 4-bit direction mask.
// bit 0 = left, bit 1 = right, bit 2 = up, bit 3 = down
static uint8 ComputeDirectionMask(float dx, float dy, float deadZone) {
	float len = sqrtf(dx * dx + dy * dy);
	if (len < deadZone)
		return 0;

	// Normalize
	float nx = dx / len;
	float ny = dy / len;

	uint8 mask = 0;

	// Use 45-degree sectors for 8-way input
	// tan(22.5) ~= 0.4142
	static const float kThreshold = 0.4142f;

	if (nx < -kThreshold) mask |= 0x01;  // left
	if (nx >  kThreshold) mask |= 0x02;  // right
	if (ny < -kThreshold) mask |= 0x04;  // up
	if (ny >  kThreshold) mask |= 0x08;  // down

	return mask;
}

// Apply direction mask change to ATInputManager
static void ApplyDirectionMask(uint8 newMask, uint8 oldMask) {
	if (!s_pInputManager)
		return;

	// Use unit 0 (same as keyboard joystick) with POV hat codes
	// POV hat is what the default input maps bind to joystick directions
	struct DirEntry {
		uint8 bit;
		uint32 code;
	};

	static const DirEntry kDirs[] = {
		{ 0x01, kATInputCode_JoyPOVLeft },
		{ 0x02, kATInputCode_JoyPOVRight },
		{ 0x04, kATInputCode_JoyPOVUp },
		{ 0x08, kATInputCode_JoyPOVDown },
	};

	for (const auto &d : kDirs) {
		bool wasDown = (oldMask & d.bit) != 0;
		bool isDown  = (newMask & d.bit) != 0;
		if (isDown && !wasDown)
			s_pInputManager->OnButtonDown(0, d.code);
		else if (!isDown && wasDown)
			s_pInputManager->OnButtonUp(0, d.code);
	}
}

// -------------------------------------------------------------------------
// Console switch helpers
// -------------------------------------------------------------------------

static void SetConsoleSwitch(uint8 bit, bool down) {
	if (s_pGTIA)
		s_pGTIA->SetConsoleSwitch(bit, down);
}

// -------------------------------------------------------------------------
// Public API
// -------------------------------------------------------------------------

void ATTouchControls_Init(ATInputManager *inputMgr, ATGTIAEmulator *gtia) {
	s_pInputManager = inputMgr;
	s_pGTIA = gtia;
	ATTouchControls_ReleaseAll();
}

void ATTouchControls_Shutdown() {
	ATTouchControls_ReleaseAll();
	s_pInputManager = nullptr;
	s_pGTIA = nullptr;
}

void ATTouchControls_ReleaseAll() {
	// Release joystick
	if (s_joyActive) {
		ApplyDirectionMask(0, s_joyDirMask);
		s_joyDirMask = 0;
		s_joyActive = false;
	}

	// Release fire buttons
	if (s_fireAHeld && s_pInputManager) {
		s_pInputManager->OnButtonUp(0, kATInputCode_JoyButton0);
		s_fireAHeld = false;
	}
	if (s_fireBHeld && s_pInputManager) {
		s_pInputManager->OnButtonUp(0, kATInputCode_JoyButton0 + 1);
		s_fireBHeld = false;
	}
	s_fireActive = false;

	// Release console switches
	if (s_startHeld)  { SetConsoleSwitch(0x01, false); s_startHeld = false; }
	if (s_selectHeld) { SetConsoleSwitch(0x02, false); s_selectHeld = false; }
	if (s_optionHeld) { SetConsoleSwitch(0x04, false); s_optionHeld = false; }
	s_consoleActive = false;

	s_menuTapped = false;
}

bool ATTouchControls_IsActive() {
	return s_joyActive || s_fireActive || s_consoleActive;
}

// -------------------------------------------------------------------------
// Event handling
// -------------------------------------------------------------------------

bool ATTouchControls_HandleEvent(const SDL_Event &ev, const ATTouchLayout &layout) {
	if (ev.type != SDL_EVENT_FINGER_DOWN &&
		ev.type != SDL_EVENT_FINGER_MOTION &&
		ev.type != SDL_EVENT_FINGER_UP)
		return false;

	// Convert normalized touch coords to pixels
	float px = ev.tfinger.x * layout.screenW;
	float py = ev.tfinger.y * layout.screenH;
	SDL_FingerID fid = ev.tfinger.fingerID;

	// Build pixel-space zone rects for hit testing
	ATTouchRect joyZonePx = ATTouchLayout_ToPixels(layout.joystickZone, layout.screenW, layout.screenH);
	ATTouchRect fireZonePx = ATTouchLayout_ToPixels(layout.fireZone, layout.screenW, layout.screenH);
	ATTouchRect topBarPx = ATTouchLayout_ToPixels(layout.topBar, layout.screenW, layout.screenH);

	if (ev.type == SDL_EVENT_FINGER_DOWN) {
		// --- MENU BUTTON ---
		if (layout.btnMenu.Contains(px, py)) {
			s_menuTapped = true;
			return true;
		}

		// --- CONSOLE KEYS (top bar) ---
		if (topBarPx.Contains(px, py) && !s_consoleActive) {
			// Only claim the finger if it actually hits a console button
			if (layout.btnStart.Contains(px, py)) {
				s_consoleFinger = fid;
				s_consoleActive = true;
				s_startHeld = true;
				SetConsoleSwitch(0x01, true);
				return true;
			} else if (layout.btnSelect.Contains(px, py)) {
				s_consoleFinger = fid;
				s_consoleActive = true;
				s_selectHeld = true;
				SetConsoleSwitch(0x02, true);
				return true;
			} else if (layout.btnOption.Contains(px, py)) {
				s_consoleFinger = fid;
				s_consoleActive = true;
				s_optionHeld = true;
				SetConsoleSwitch(0x04, true);
				return true;
			}
			// Touch in top bar but not on any button — don't consume
		}

		// --- JOYSTICK ZONE (left side) ---
		if (joyZonePx.Contains(px, py) && !s_joyActive) {
			s_joyFinger = fid;
			s_joyActive = true;
			s_joyBaseX = px;
			s_joyBaseY = py;
			s_joyCurX = px;
			s_joyCurY = py;
			s_joyDirMask = 0;
			return true;
		}

		// --- FIRE BUTTON ZONE (right side) ---
		if (fireZonePx.Contains(px, py) && !s_fireActive) {
			s_fireFinger = fid;
			s_fireActive = true;

			if (layout.btnFireA.Contains(px, py)) {
				s_fireAHeld = true;
				if (s_pInputManager)
					s_pInputManager->OnButtonDown(0, kATInputCode_JoyButton0);
			}
			if (layout.btnFireB.Contains(px, py)) {
				s_fireBHeld = true;
				if (s_pInputManager)
					s_pInputManager->OnButtonDown(0, kATInputCode_JoyButton0 + 1);
			}
			// If finger lands between buttons, default to Fire A
			if (!s_fireAHeld && !s_fireBHeld) {
				s_fireAHeld = true;
				if (s_pInputManager)
					s_pInputManager->OnButtonDown(0, kATInputCode_JoyButton0);
			}
			return true;
		}

		return false;
	}

	if (ev.type == SDL_EVENT_FINGER_MOTION) {
		// --- JOYSTICK MOTION ---
		if (s_joyActive && fid == s_joyFinger) {
			s_joyCurX = px;
			s_joyCurY = py;

			float dx = s_joyCurX - s_joyBaseX;
			float dy = s_joyCurY - s_joyBaseY;

			// Clamp to max radius
			float len = sqrtf(dx * dx + dy * dy);
			if (len > layout.joyMaxRadius) {
				dx = dx / len * layout.joyMaxRadius;
				dy = dy / len * layout.joyMaxRadius;
				s_joyCurX = s_joyBaseX + dx;
				s_joyCurY = s_joyBaseY + dy;
			}

			uint8 newMask = ComputeDirectionMask(dx, dy, layout.joyDeadZone);
			if (newMask != s_joyDirMask) {
				ApplyDirectionMask(newMask, s_joyDirMask);
				s_joyDirMask = newMask;
			}
			return true;
		}

		// Fire finger motion — allow sliding between fire A and fire B
		if (s_fireActive && fid == s_fireFinger) {
			bool nowA = layout.btnFireA.Contains(px, py);
			bool nowB = layout.btnFireB.Contains(px, py);

			if (nowA && !s_fireAHeld) {
				s_fireAHeld = true;
				if (s_pInputManager) s_pInputManager->OnButtonDown(0, kATInputCode_JoyButton0);
			} else if (!nowA && s_fireAHeld) {
				s_fireAHeld = false;
				if (s_pInputManager) s_pInputManager->OnButtonUp(0, kATInputCode_JoyButton0);
			}

			if (nowB && !s_fireBHeld) {
				s_fireBHeld = true;
				if (s_pInputManager) s_pInputManager->OnButtonDown(0, kATInputCode_JoyButton0 + 1);
			} else if (!nowB && s_fireBHeld) {
				s_fireBHeld = false;
				if (s_pInputManager) s_pInputManager->OnButtonUp(0, kATInputCode_JoyButton0 + 1);
			}
			return true;
		}

		return false;
	}

	if (ev.type == SDL_EVENT_FINGER_UP) {
		// --- JOYSTICK RELEASE ---
		if (s_joyActive && fid == s_joyFinger) {
			ApplyDirectionMask(0, s_joyDirMask);
			s_joyDirMask = 0;
			s_joyActive = false;
			return true;
		}

		// --- FIRE RELEASE ---
		if (s_fireActive && fid == s_fireFinger) {
			if (s_fireAHeld && s_pInputManager) {
				s_pInputManager->OnButtonUp(0, kATInputCode_JoyButton0);
				s_fireAHeld = false;
			}
			if (s_fireBHeld && s_pInputManager) {
				s_pInputManager->OnButtonUp(0, kATInputCode_JoyButton0 + 1);
				s_fireBHeld = false;
			}
			s_fireActive = false;
			return true;
		}

		// --- CONSOLE KEY RELEASE ---
		if (s_consoleActive && fid == s_consoleFinger) {
			if (s_startHeld)  { SetConsoleSwitch(0x01, false); s_startHeld = false; }
			if (s_selectHeld) { SetConsoleSwitch(0x02, false); s_selectHeld = false; }
			if (s_optionHeld) { SetConsoleSwitch(0x04, false); s_optionHeld = false; }
			s_consoleActive = false;
			return true;
		}

		return false;
	}

	return false;
}

// -------------------------------------------------------------------------
// Rendering
// -------------------------------------------------------------------------

// Draw a rounded-rect button with label
static void DrawButton(ImDrawList *dl, const ATTouchRect &r, const char *label,
	ImU32 color, ImU32 textColor, bool pressed)
{
	ImU32 fillColor = pressed
		? IM_COL32(
			(color >> IM_COL32_R_SHIFT & 0xFF),
			(color >> IM_COL32_G_SHIFT & 0xFF),
			(color >> IM_COL32_B_SHIFT & 0xFF),
			200)
		: color;

	float rounding = (r.x1 - r.x0) * 0.15f;
	dl->AddRectFilled(ImVec2(r.x0, r.y0), ImVec2(r.x1, r.y1), fillColor, rounding);
	dl->AddRect(ImVec2(r.x0, r.y0), ImVec2(r.x1, r.y1),
		IM_COL32(255, 255, 255, 100), rounding, 0, 2.0f);

	// Center label text
	ImVec2 textSize = ImGui::CalcTextSize(label);
	float tx = r.CenterX() - textSize.x * 0.5f;
	float ty = r.CenterY() - textSize.y * 0.5f;
	dl->AddText(ImVec2(tx, ty), textColor, label);
}

// Draw circular joystick base and knob
static void DrawJoystick(ImDrawList *dl, float baseX, float baseY,
	float curX, float curY, float maxRadius, float opacity)
{
	ImU32 baseColor = IM_COL32(255, 255, 255, (int)(50 * opacity));
	ImU32 knobColor = IM_COL32(255, 255, 255, (int)(120 * opacity));
	ImU32 ringColor = IM_COL32(255, 255, 255, (int)(80 * opacity));

	// Outer ring (max extent)
	dl->AddCircle(ImVec2(baseX, baseY), maxRadius, ringColor, 48, 2.0f);

	// Base circle
	dl->AddCircleFilled(ImVec2(baseX, baseY), maxRadius * 0.85f, baseColor, 48);

	// Knob (follows finger)
	float knobRadius = maxRadius * 0.35f;
	dl->AddCircleFilled(ImVec2(curX, curY), knobRadius, knobColor, 32);
}

void ATTouchControls_Render(const ATTouchLayout &layout, const ATTouchLayoutConfig &config) {
	float alpha = config.controlOpacity;
	ImU32 btnNormal    = IM_COL32(80, 80, 80, (int)(140 * alpha));
	ImU32 btnConsole   = IM_COL32(60, 60, 80, (int)(140 * alpha));
	ImU32 btnFireA     = IM_COL32(180, 40, 40, (int)(160 * alpha));
	ImU32 btnFireB     = IM_COL32(40, 80, 180, (int)(160 * alpha));
	ImU32 textColor    = IM_COL32(255, 255, 255, (int)(220 * alpha));
	ImU32 menuColor    = IM_COL32(200, 200, 200, (int)(180 * alpha));

	// Get foreground draw list (renders on top of everything)
	ImDrawList *dl = ImGui::GetForegroundDrawList();

	// --- Console keys ---
	DrawButton(dl, layout.btnStart, "START", btnConsole, textColor, s_startHeld);
	DrawButton(dl, layout.btnSelect, "SELECT", btnConsole, textColor, s_selectHeld);
	DrawButton(dl, layout.btnOption, "OPTION", btnConsole, textColor, s_optionHeld);

	// --- Hamburger menu icon ---
	{
		const ATTouchRect &m = layout.btnMenu;
		float cx = m.CenterX();
		float cy = m.CenterY();
		float hw = m.Width() * 0.3f;
		float lineSpacing = m.Height() * 0.18f;
		float thickness = 2.5f;

		for (int i = -1; i <= 1; i++) {
			float ly = cy + i * lineSpacing;
			dl->AddLine(ImVec2(cx - hw, ly), ImVec2(cx + hw, ly), menuColor, thickness);
		}
	}

	// --- Fire buttons ---
	DrawButton(dl, layout.btnFireA, "A", btnFireA, textColor, s_fireAHeld);
	DrawButton(dl, layout.btnFireB, "B", btnFireB, textColor, s_fireBHeld);

	// --- Joystick ---
	if (s_joyActive) {
		DrawJoystick(dl, s_joyBaseX, s_joyBaseY,
			s_joyCurX, s_joyCurY,
			layout.joyMaxRadius, alpha);
	} else {
		// When inactive, show a subtle hint circle in the center of the joy zone
		ATTouchRect joyPx = ATTouchLayout_ToPixels(layout.joystickZone, layout.screenW, layout.screenH);
		float hintX = joyPx.CenterX();
		float hintY = joyPx.CenterY();
		ImU32 hintColor = IM_COL32(255, 255, 255, (int)(30 * alpha));
		dl->AddCircle(ImVec2(hintX, hintY), layout.joyMaxRadius * 0.6f, hintColor, 32, 1.5f);
	}
}
