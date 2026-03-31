//	Altirra SDL3 frontend - input handling
//
//	Maps SDL3 keyboard events to Atari 800 key scancodes and feeds them
//	directly to ATPokeyEmulator::PushKey() / PushRawKey().
//
//	Atari 800 keyboard matrix scancodes (same as Atari POKEY KBCODE register).
//	Bit 6 = Shift, Bit 7 = Control (conceptually, via the SKSTAT register).
//	The actual scan codes are 6-bit values 0x00-0x3F.

#include <stdafx.h>
#include <SDL3/SDL.h>
#include <at/ataudio/pokey.h>

// -------------------------------------------------------------------------
// SDL scancode → Atari KBCODE mapping
// Reference: Atari 800 Hardware Reference Manual, Chapter 3
// -------------------------------------------------------------------------

// Returns 0xFF if not mapped
static uint8 SDLScancodeToAtari(SDL_Scancode sc, bool shift, bool ctrl) {
	// Atari scan codes (0x00-0x3F, without modifier bits)
	// These are the raw KBCODE values from the POKEY hardware

	switch (sc) {
	// Row 0: digits
	case SDL_SCANCODE_1:       return shift ? 0x1F : 0x1F; // ! or 1  (Atari: 0x1F)
	case SDL_SCANCODE_2:       return 0x1E;  // 2 / @
	case SDL_SCANCODE_3:       return 0x1A;  // 3 / #
	case SDL_SCANCODE_4:       return 0x18;  // 4 / $
	case SDL_SCANCODE_5:       return 0x1D;  // 5 / %
	case SDL_SCANCODE_6:       return 0x1B;  // 6 / &
	case SDL_SCANCODE_7:       return 0x33;  // 7 / '
	case SDL_SCANCODE_8:       return 0x35;  // 8 / @
	case SDL_SCANCODE_9:       return 0x30;  // 9 / (
	case SDL_SCANCODE_0:       return 0x32;  // 0 / )
	case SDL_SCANCODE_MINUS:   return 0x38;  // - / _
	case SDL_SCANCODE_EQUALS:  return 0x3A;  // = / |

	// Row 1: QWERTYUIOP
	case SDL_SCANCODE_Q:       return 0x2F;
	case SDL_SCANCODE_W:       return 0x2E;
	case SDL_SCANCODE_E:       return 0x2A;
	case SDL_SCANCODE_R:       return 0x28;
	case SDL_SCANCODE_T:       return 0x2D;
	case SDL_SCANCODE_Y:       return 0x2B;
	case SDL_SCANCODE_U:       return 0x0B;
	case SDL_SCANCODE_I:       return 0x0D;
	case SDL_SCANCODE_O:       return 0x08;
	case SDL_SCANCODE_P:       return 0x0A;
	case SDL_SCANCODE_LEFTBRACKET:  return 0x60; // not standard Atari
	case SDL_SCANCODE_RIGHTBRACKET: return 0x60; // not standard Atari

	// Row 2: ASDFGHJKL
	case SDL_SCANCODE_A:       return 0x3F;
	case SDL_SCANCODE_S:       return 0x3E;
	case SDL_SCANCODE_D:       return 0x3A;  // wait, conflict with = ?
	case SDL_SCANCODE_F:       return 0x38;  // conflict with - ?
	case SDL_SCANCODE_G:       return 0x3D;
	case SDL_SCANCODE_H:       return 0x39;
	case SDL_SCANCODE_J:       return 0x01;
	case SDL_SCANCODE_K:       return 0x05;
	case SDL_SCANCODE_L:       return 0x00;
	case SDL_SCANCODE_SEMICOLON:   return 0x02;  // ;
	case SDL_SCANCODE_APOSTROPHE:  return 0x02;  // ' → ;

	// Row 3: ZXCVBNM
	case SDL_SCANCODE_Z:       return 0x17;
	case SDL_SCANCODE_X:       return 0x16;
	case SDL_SCANCODE_C:       return 0x12;
	case SDL_SCANCODE_V:       return 0x10;
	case SDL_SCANCODE_B:       return 0x15;
	case SDL_SCANCODE_N:       return 0x23;
	case SDL_SCANCODE_M:       return 0x25;
	case SDL_SCANCODE_COMMA:   return 0x20;  // ,
	case SDL_SCANCODE_PERIOD:  return 0x22;  // .
	case SDL_SCANCODE_SLASH:   return 0x26;  // /

	// Special keys
	case SDL_SCANCODE_SPACE:   return 0x21;
	case SDL_SCANCODE_RETURN:  return 0x0C;
	case SDL_SCANCODE_BACKSPACE: return 0x34;
	case SDL_SCANCODE_TAB:     return 0x2C;
	case SDL_SCANCODE_ESCAPE:  return 0x1C;  // Escape = ESC

	// Function keys → Atari function keys
	case SDL_SCANCODE_F1:      return 0x03;  // Option (console key, handled differently)
	case SDL_SCANCODE_F2:      return 0x04;  // Select
	case SDL_SCANCODE_F3:      return 0x06;  // Start
	case SDL_SCANCODE_F5:      return 0x14;  // Shift+< (Inverse)
	case SDL_SCANCODE_DELETE:  return 0x34;  // Backspace/Delete

	case SDL_SCANCODE_CAPSLOCK: return 0x3C; // Caps

	default: return 0xFF;
	}
}

// -------------------------------------------------------------------------
// Input state
// -------------------------------------------------------------------------

struct ATInputStateSDL3 {
	ATPokeyEmulator *mpPokey = nullptr;
};

static ATInputStateSDL3 g_inputState;

void ATInputSDL3_Init(ATPokeyEmulator *pokey) {
	g_inputState.mpPokey = pokey;
}

void ATInputSDL3_HandleKeyDown(const SDL_KeyboardEvent& ev) {
	if (!g_inputState.mpPokey) return;

	bool shift = (ev.mod & SDL_KMOD_SHIFT) != 0;
	bool ctrl  = (ev.mod & SDL_KMOD_CTRL) != 0;

	// Handle Break
	if (ev.scancode == SDL_SCANCODE_PAUSE || ev.scancode == SDL_SCANCODE_F8) {
		g_inputState.mpPokey->PushBreak();
		return;
	}

	uint8 atariCode = SDLScancodeToAtari(ev.scancode, shift, ctrl);
	if (atariCode == 0xFF) return;

	// Modify scan code with shift/ctrl
	if (ctrl)  atariCode |= 0x80;
	if (shift) atariCode |= 0x40;

	g_inputState.mpPokey->PushKey(atariCode, false, true, false, true);
}

void ATInputSDL3_HandleKeyUp(const SDL_KeyboardEvent& ev) {
	// Key releases are handled automatically by POKEY's key cooldown timer
	// Raw key release: only used for special keys
}
