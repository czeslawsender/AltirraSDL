# Input Handling

## Current Architecture (Windows)

```
Windows Messages (WM_KEYDOWN, WM_MOUSEMOVE, WM_INPUT)
    |
    v
ATUIManager::OnKeyDown/OnMouseMove/etc.
    |  (dispatches to UI or emulation)
    v
ATInputManager
    |  (maps host input codes to Atari inputs)
    v
Port Controllers (Joystick, Paddle, Keyboard mode)
    |
    v
GTIA / POKEY (hardware ports and keyboard matrix)
```

Input enters through Windows messages, is translated to `ATInputCode` values
(defined in `inputdefs.h`), and mapped to Atari controller inputs through
the `ATInputManager`.

### ATInputCode System

Defined in `src/Altirra/h/inputdefs.h`:

- `0x00-0x7B`: Keyboard keys (based on Windows virtual key codes)
- `0x100+`: Extended keys
- `0x1000-0x1FFF`: Mouse inputs (position, buttons, wheel)
- `0x2000+`: Joystick/gamepad inputs (axes, buttons)

The input manager has a hash table mapping `ATInputCode` -> Atari actions.
Users configure these mappings (e.g., "arrow keys = joystick directions").

## SDL3 Architecture

```
SDL_Event loop (SDL_EVENT_KEY_DOWN, SDL_EVENT_MOUSE_MOTION, SDL_EVENT_GAMEPAD_*)
    |
    v
SDL3-to-ATInputCode translation layer
    |
    v
ATInputManager (unchanged)
    |
    v
Port Controllers (unchanged)
    |
    v
GTIA / POKEY (unchanged)
```

The entire chain from `ATInputManager` down is platform-agnostic. We only
need to replace the top layer: translating SDL3 events into `ATInputCode`
values and feeding them to the input manager.

### SDL3 Event Translation

Create a translation function in the SDL3 frontend:

```cpp
ATInputCode TranslateSDL3Key(SDL_Scancode scancode) {
    // Map SDL3 scancodes to ATInputCode values
    // ATInputCode keyboard range is 0x00-0x7B, based on Win32 VK codes
    static const ATInputCode sScancodeMap[] = {
        // SDL_SCANCODE_A..Z -> kATInputCode_A..Z (0x41-0x5A)
        // SDL_SCANCODE_0..9 -> kATInputCode_0..9 (0x30-0x39)
        // SDL_SCANCODE_RETURN -> kATInputCode_Return (0x0D)
        // SDL_SCANCODE_ESCAPE -> kATInputCode_Escape (0x1B)
        // etc.
    };
    // ...
}
```

The mapping table is mechanical: SDL3 scancodes are standardized USB HID
codes, and ATInputCode values are Windows virtual key codes. The mapping
between the two is well-defined and static.

### Event Processing in Main Loop

```cpp
void ProcessSDL3Input(ATInputManager& inputManager) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_KEY_DOWN: {
            ATInputCode code = TranslateSDL3Key(event.key.scancode);
            if (code)
                inputManager.OnKeyDown(code);
            break;
        }
        case SDL_EVENT_KEY_UP: {
            ATInputCode code = TranslateSDL3Key(event.key.scancode);
            if (code)
                inputManager.OnKeyUp(code);
            break;
        }
        case SDL_EVENT_MOUSE_MOTION:
            inputManager.OnMouseMove(event.motion.x, event.motion.y);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            inputManager.OnMouseButtonDown(event.button.button);
            break;
        // ... etc.
        }
    }
}
```

### Gamepad / Joystick

SDL3 has excellent gamepad support with automatic controller mapping. The
`ATInputManager` already supports joystick input codes (`0x2000+` range).

```cpp
case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
    // Map SDL3 gamepad axis to ATInputCode joystick axis
    ATInputCode code = TranslateSDL3GamepadAxis(event.gaxis.axis);
    inputManager.OnJoystickAxis(code, event.gaxis.value / 32767.0f);
    break;
}
case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
    ATInputCode code = TranslateSDL3GamepadButton(event.gbutton.button);
    inputManager.OnButtonDown(code);
    break;
}
```

SDL3 handles controller hotplug, rumble, and DB mapping automatically.
The Windows build uses `IATJoystickManager` for this; the SDL3 build
can implement the same interface using SDL3 gamepad APIs or bypass it
and feed input codes directly to `ATInputManager`.

### Mouse Capture (Light Pen, Paddle)

Some Atari peripherals (light pen, mouse, paddle) require relative mouse
motion. SDL3 supports this via `SDL_SetWindowRelativeMouseMode()`:

```cpp
void EnableMouseCapture(SDL_Window *window) {
    SDL_SetWindowRelativeMouseMode(window, true);
}
```

The `IATUINativeDisplay::CaptureCursor()` and `ReleaseCursor()` calls from
the UI manager translate to `SDL_SetWindowRelativeMouseMode()`.

### Keyboard Text Input

For the Atari keyboard (typing characters), SDL3 provides
`SDL_EVENT_TEXT_INPUT` which gives UTF-8 text. This handles keyboard
layouts, dead keys, and IME. Feed this to the emulator's keyboard handler
for character-based input (as opposed to scancode-based game input).

## Key Mapping Table (Excerpt)

| SDL3 Scancode | ATInputCode | Atari Function |
|---------------|-------------|----------------|
| `SDL_SCANCODE_A`-`Z` | `0x41`-`0x5A` | Letters |
| `SDL_SCANCODE_0`-`9` | `0x30`-`0x39` | Numbers |
| `SDL_SCANCODE_RETURN` | `0x0D` | Return |
| `SDL_SCANCODE_ESCAPE` | `0x1B` | Escape |
| `SDL_SCANCODE_BACKSPACE` | `0x08` | Backspace (Delete on Atari) |
| `SDL_SCANCODE_TAB` | `0x09` | Tab |
| `SDL_SCANCODE_SPACE` | `0x20` | Space |
| `SDL_SCANCODE_UP` | `0x26` | Up arrow / Joystick up |
| `SDL_SCANCODE_DOWN` | `0x28` | Down arrow / Joystick down |
| `SDL_SCANCODE_LEFT` | `0x25` | Left arrow / Joystick left |
| `SDL_SCANCODE_RIGHT` | `0x27` | Right arrow / Joystick right |
| `SDL_SCANCODE_F1`-`F12` | `0x70`-`0x7B` | Function keys (emulator controls) |
| `SDL_SCANCODE_LSHIFT` | `0x10` | Shift |
| `SDL_SCANCODE_LCTRL` | `0x11` | Control |
| `SDL_SCANCODE_LALT` | `0x12` | Option (Atari) |

The full mapping is ~100 entries and can be built from the Win32 VK code
constants that `ATInputCode` is based on.

## Summary of New Files

| File | Purpose |
|------|---------|
| `src/AltirraSDL/source/input_sdl3.cpp` | SDL3 event → ATInputCode translation and dispatch |
| `src/AltirraSDL/h/input_sdl3.h` | Header for the above |

## Interface Dependency

Depends only on:

- `ATInputManager` (in `Altirra/h/inputmanager.h`)
- `ATInputCode` definitions (in `Altirra/h/inputdefs.h`)
- SDL3 event headers

The `ATInputManager` and all downstream emulation (port controllers, GTIA,
POKEY keyboard) are unchanged.
