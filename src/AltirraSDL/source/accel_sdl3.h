//	AltirraSDL - SDL3 accelerator table management
//	Data-driven keyboard shortcut dispatch, matching Windows ATUIActivateVirtKeyMapping.

#pragma once

#include <SDL3/SDL.h>
#include <vd2/system/vdtypes.h>
#include "uikeyboard.h"

// Initialize command handlers for all shortcut-driven commands.
// Must be called before ATUIInitDefaultAccelTables().
void ATUIInitSDL3Commands();

// Convert SDL keyboard event to VK+modifiers and dispatch through accel tables.
// Returns true if a command was executed.
bool ATUISDLActivateAccelKey(const SDL_KeyboardEvent& ev, bool up, ATUIAccelContext context);

// Convert SDL scancode to Windows VK code (same mapping as SDLScancodeToInputCode).
uint32 SDLScancodeToVK(SDL_Scancode sc);

// Check if a VK code with given SDL modifiers is bound in any accel table.
// Used to suppress emulator key-up for bound keys.
bool ATUIFindBoundKey(uint32 vk, SDL_Keymod sdlMod, SDL_Scancode scancode);

// Get cached UTF-8 shortcut string for a command (for menu display).
// Searches all contexts. Returns "" if not found.
const char *ATUIGetShortcutStringForCommand(const char *command);

// Invalidate the shortcut string cache (call after modifying accel tables).
void ATUIInvalidateShortcutCache();

// Shortcut capture mode — for rebinding UI
extern bool g_shortcutCaptureActive;

struct ATShortcutCaptureResult {
	bool captured;
	uint32 vk;
	uint32 modifiers;  // VDUIAccelerator modifier flags
};

extern ATShortcutCaptureResult g_shortcutCaptureResult;

// Handle a key event during shortcut capture mode.
void ATUIHandleShortcutCapture(const SDL_KeyboardEvent& ev);

// Open the shortcut editor with a specific command pre-selected for rebinding.
// If command is nullptr, opens normally.
void ATUIOpenShortcutEditor(const char *command);

// Pending shortcut assign target (set by right-click "Assign Keyboard Shortcut...")
extern VDStringA g_pendingShortcutAssign;
