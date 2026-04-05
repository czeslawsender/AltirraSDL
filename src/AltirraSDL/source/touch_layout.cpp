//	AltirraSDL - Adaptive touch control layout engine
//	Calculates pixel positions for all touch controls based on screen
//	dimensions, orientation, and user-configured control size.
//	All sizing uses density-independent pixels (dp) scaled by contentScale.

#include <stdafx.h>
#include "touch_layout.h"
#include <algorithm>
#include <cmath>

ATTouchRect ATTouchLayout_ToPixels(const ATTouchRect &norm, int screenW, int screenH) {
	return {
		norm.x0 * screenW,
		norm.y0 * screenH,
		norm.x1 * screenW,
		norm.y1 * screenH
	};
}

// Helper: create a pixel-coordinate button rect centered at (cx, cy) with given size
static ATTouchRect MakeButton(float cx, float cy, float w, float h) {
	return { cx - w * 0.5f, cy - h * 0.5f, cx + w * 0.5f, cy + h * 0.5f };
}

void ATTouchLayout_Update(ATTouchLayout &layout, int screenW, int screenH,
	const ATTouchLayoutConfig &config)
{
	layout.screenW = screenW;
	layout.screenH = screenH;
	layout.landscape = (screenW > screenH);
	layout.lastControlSize = config.controlSize;
	layout.lastContentScale = config.contentScale;

	// dp-to-pixel conversion using display content scale
	float cs = config.contentScale;
	if (cs < 1.0f) cs = 1.0f;
	auto dp = [cs](float v) -> float { return v * cs; };

	// Scale factor based on control size setting
	float sizeMult = 1.0f;
	switch (config.controlSize) {
	case ATTouchControlSize::Small:  sizeMult = 0.8f;  break;
	case ATTouchControlSize::Medium: sizeMult = 1.0f;  break;
	case ATTouchControlSize::Large:  sizeMult = 1.25f; break;
	}

	// Console button dimensions (dp-based)
	// Width: 72dp, Height: 36dp visual but 48dp touch target
	float consoleBtnW = dp(72.0f * sizeMult);
	float consoleBtnH = dp(36.0f * sizeMult);
	float menuBtnSize = dp(48.0f * sizeMult);

	// Fire button dimensions — 56dp for primary, 48dp for secondary
	float fireASize = dp(56.0f * sizeMult);
	float fireBSize = dp(48.0f * sizeMult);

	// Joystick parameters — 56dp max radius
	layout.joyMaxRadius = dp(56.0f * sizeMult);
	layout.joyDeadZone  = layout.joyMaxRadius * 0.15f;

	// Top bar height: 56dp
	float topBarH = dp(56.0f);

	if (layout.landscape) {
		// ---- LANDSCAPE LAYOUT ----
		float topBarNorm = topBarH / screenH;

		// Normalized zones
		layout.topBar       = { 0.0f, 0.0f, 1.0f, topBarNorm };
		layout.joystickZone = { 0.0f, topBarNorm, 0.30f, 1.0f };
		layout.fireZone     = { 0.78f, 0.40f, 1.0f, 1.0f };
		layout.displayArea  = { 0.12f, topBarNorm, 0.80f, 1.0f };

		// Console keys (top-left, evenly spaced)
		float topCenterY = topBarH * 0.5f;
		float consoleStartX = dp(16.0f) + consoleBtnW * 0.5f;
		float consoleSpacing = consoleBtnW + dp(8.0f);

		layout.btnStart  = MakeButton(consoleStartX, topCenterY, consoleBtnW, consoleBtnH);
		layout.btnSelect = MakeButton(consoleStartX + consoleSpacing, topCenterY, consoleBtnW, consoleBtnH);
		layout.btnOption = MakeButton(consoleStartX + consoleSpacing * 2, topCenterY, consoleBtnW, consoleBtnH);

		// Hamburger icon (top-right)
		layout.btnMenu = MakeButton(screenW - dp(16.0f) - menuBtnSize * 0.5f, topCenterY, menuBtnSize, menuBtnSize);

		// Fire buttons (right side, stacked vertically)
		float fireCenterX = screenW * 0.90f;
		float fireSpacing = dp(16.0f);
		float fireAY = screenH * 0.78f;
		float fireBY = fireAY - fireASize * 0.5f - fireSpacing - fireBSize * 0.5f;
		layout.btnFireA = MakeButton(fireCenterX, fireAY, fireASize, fireASize);
		layout.btnFireB = MakeButton(fireCenterX, fireBY, fireBSize, fireBSize);

	} else {
		// ---- PORTRAIT LAYOUT ----
		float topBarNorm = topBarH / screenH;
		float controlsTop = 0.68f;

		// Normalized zones
		layout.topBar       = { 0.0f, 0.0f, 1.0f, topBarNorm };
		layout.joystickZone = { 0.0f, controlsTop, 0.50f, 1.0f };
		layout.fireZone     = { 0.60f, controlsTop, 1.0f, 1.0f };
		layout.displayArea  = { 0.0f, topBarNorm, 1.0f, controlsTop };

		// Console keys (top, left-aligned)
		float topCenterY = topBarH * 0.5f;
		float consoleStartX = dp(12.0f) + consoleBtnW * 0.5f;
		float consoleSpacing = consoleBtnW + dp(6.0f);

		layout.btnStart  = MakeButton(consoleStartX, topCenterY, consoleBtnW, consoleBtnH);
		layout.btnSelect = MakeButton(consoleStartX + consoleSpacing, topCenterY, consoleBtnW, consoleBtnH);
		layout.btnOption = MakeButton(consoleStartX + consoleSpacing * 2, topCenterY, consoleBtnW, consoleBtnH);

		// Hamburger icon (top-right)
		layout.btnMenu = MakeButton(screenW - dp(12.0f) - menuBtnSize * 0.5f, topCenterY, menuBtnSize, menuBtnSize);

		// Fire buttons (bottom-right, stacked)
		float fireCenterX = screenW * 0.82f;
		float fireSpacing = dp(16.0f);
		float fireAY = screenH * 0.88f;
		float fireBY = fireAY - fireASize * 0.5f - fireSpacing - fireBSize * 0.5f;
		layout.btnFireA = MakeButton(fireCenterX, fireAY, fireASize, fireASize);
		layout.btnFireB = MakeButton(fireCenterX, fireBY, fireBSize, fireBSize);
	}
}
