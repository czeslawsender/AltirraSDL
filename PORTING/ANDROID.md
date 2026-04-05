# Android Platform Support

## Goal

Run Altirra on Android phones and tablets with a streamlined, touch-first
UI designed for playing games. The Android build is a compile-time variant
of the existing AltirraSDL frontend -- it shares the same emulation core,
display backend, and input manager. No separate application.

## Design Principles

1. **Touch-first UX.** Every interaction must work with fingers on a
   touchscreen. No hover states, no tiny hit targets, no desktop
   assumptions.

2. **Streamlined, not stripped.** The mobile UI exposes a curated subset
   of settings (PAL/NTSC, memory size, BASIC toggle, disk drives, audio,
   reset). Advanced configuration remains available on desktop. The goal
   is a great game-playing experience, not a portable settings editor.

3. **Existing infrastructure.** Touch controls feed into `ATInputManager`
   and the POKEY keyboard path -- the same code paths that physical
   gamepads and keyboards use. Zero changes to the emulation core.

4. **Resolution and orientation independence.** All control positions
   are expressed as fractions of screen dimensions. Layout adapts to
   portrait, landscape, and runtime rotation.

5. **ImGui overlay.** Touch controls and menus render as ImGui draw
   commands on top of the emulator display, integrated into the existing
   `ATUIRenderFrame()` pipeline. This gives consistent theming, proper
   text rendering, and alpha blending.

6. **DPI-aware sizing.** All touch targets and UI elements are sized in
   density-independent pixels (dp), not raw pixels. A 48dp button is
   physically the same size on a 440-DPI phone and a 240-DPI tablet.
   This is the single most important rule for a usable mobile UI.

7. **Safe area respect.** Controls and UI chrome never overlap notches,
   punch-holes, rounded corners, or system navigation bars. The layout
   queries safe area insets and offsets all content inward.

## DPI and Sizing Foundation

### The dp Unit

All measurements in this document are in **dp** (density-independent
pixels). 1 dp = 1 physical pixel at 160 DPI (mdpi baseline). On a
typical 440-DPI phone, 1 dp ≈ 2.75 physical pixels.

At startup, compute the dp scale factor:

```cpp
float contentScale = SDL_GetDisplayContentScale(SDL_GetDisplayForWindow(window));
// contentScale is the ratio: physical pixels per dp
// Typical values: 1.0 (mdpi), 1.5 (hdpi), 2.0 (xhdpi),
//                 2.5 (xxhdpi), 3.0 (xxxhdpi), 3.5+ (some flagships)

// Convert dp to pixels:
float dp(float dpValue) { return dpValue * contentScale; }
```

All sizes below are given in dp. The implementation must multiply by
the content scale to get pixel values. Never use raw pixel constants.

### Touch Target Minimums (Material Design)

| Target type | Minimum | Recommended | Used here |
|-------------|---------|-------------|-----------|
| Primary action (fire buttons) | 48dp | 56dp | 56dp |
| Secondary action (console keys) | 48dp | 48dp | 48dp tall, 72dp wide |
| Menu/list item row | 48dp | 56dp | 56dp |
| Icon button (menu, back) | 48dp | 48dp | 48dp |
| Spacing between targets | 8dp | 8dp | 8dp minimum |
| Slider thumb | 20dp | 24dp | 24dp radius |

These are **hit target** sizes, not visual sizes. A button can look
smaller than 48dp if its tappable area extends beyond its visual bounds
(padding). But the tappable area itself must meet the minimum.

### Safe Area Insets

Modern Android phones have notches, punch-holes, rounded corners, and
gesture navigation bars that eat into the screen. The layout engine
must respect these.

```cpp
// SDL3 provides safe area via display usable bounds
SDL_Rect usable;
SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(window), &usable);

// On Android, also query window safe area insets via JNI or
// SDL3's window properties for more precise cutout avoidance.
// The top bar and all controls must be inset by at least these amounts.

struct SafeInsets {
    float top, bottom, left, right;  // in pixels
};
```

All layout zones are offset inward by the safe insets. The top bar
starts below the top inset; bottom controls end above the bottom
inset; left/right zones are shifted inward by their respective insets.

## Screen Layout

### Landscape (primary gaming orientation)

```
+----------------------------------------------------------------+
|▓▓▓▓▓▓▓▓▓▓▓▓▓▓ safe inset top ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓|
|▓| [START] [SELECT] [OPTION]                          [☰ Menu] |▓|
|▓|                                                              |▓|
|▓|                                                              |▓|
|▓|                     EMULATOR DISPLAY                         |▓|
|▓|                  (aspect-ratio preserved,                    |▓|
|▓|                   centered, black bars)                      |▓|
|▓|                                                              |▓|
|▓|    ,---.                                          [FIRE B]   |▓|
|▓|   ( Joy )                                         [FIRE A]   |▓|
|▓|    `---'                                                     |▓|
|▓▓▓▓▓▓▓▓▓▓▓▓▓▓ safe inset bottom ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓|
+----------------------------------------------------------------+
```

### Portrait

```
+----------------------------------+
|▓▓▓▓ safe inset top ▓▓▓▓▓▓▓▓▓▓▓▓▓|
| [START][SEL][OPT]       [☰ Menu] |
|                                   |
|                                   |
|        EMULATOR DISPLAY           |
|        (letterboxed,              |
|         aspect-ratio kept)        |
|                                   |
|                                   |
|     ,---.                 [FB]    |
|    ( Joy )                [FA]    |
|     `---'                         |
|▓▓▓▓ safe inset bottom ▓▓▓▓▓▓▓▓▓▓|
+----------------------------------+
```

### Layout Zones

The screen is divided into non-overlapping zones calculated as fractions
of the **safe area** (screen minus insets). The emulator output occupies
the center, preserving its native aspect ratio.

```
Zone map (landscape, after safe insets applied):

  +--[TOP BAR: 56dp height]--------------------------+
  |                                                   |
  |  [LEFT: 30% width]  [CENTER]  [RIGHT: 22% width] |
  |  joystick zone        display   fire buttons zone |
  |                                                   |
  +---------------------------------------------------+
```

In portrait, the bottom 32% of the safe area is reserved for controls,
and the display fills the remaining area above.

Zone percentages are configurable (see Settings below).

### Top Bar Behavior

The top bar (console keys + menu button) is **semi-transparent** and
**auto-hides** after 3 seconds of inactivity to maximize display area:

- On any touch event: reset the 3-second timer, fade the bar in
  (150ms ease-in)
- After 3 seconds with no touch: fade the bar out (300ms ease-out)
- While faded out, the bar's hit area remains active (invisible but
  tappable). A tap in the top-bar zone first reveals the bar, then
  a second tap activates the button underneath.
- While the hamburger menu, file browser, or settings are open, the
  auto-hide timer is suspended and the bar is always visible.
- The bar uses a gradient fade at its bottom edge (not a hard line)
  to blend smoothly with the game display.

```cpp
// Auto-hide state
float topBarAlpha = 1.0f;       // current opacity (0.0 = hidden, 1.0 = visible)
float topBarTimer = 3.0f;       // seconds remaining before fade-out
bool  topBarRevealing = false;  // true during the "first tap reveals" state

// Each frame:
if (anyTouchThisFrame) {
    topBarTimer = 3.0f;
    // Fade in over 150ms
    topBarAlpha = std::min(1.0f, topBarAlpha + deltaTime / 0.15f);
} else {
    topBarTimer -= deltaTime;
    if (topBarTimer <= 0) {
        // Fade out over 300ms
        topBarAlpha = std::max(0.0f, topBarAlpha - deltaTime / 0.30f);
    }
}
```

## Touch Controls

### Virtual Joystick (default)

Appears where the left thumb touches within the left control zone.
A base circle renders at the touch-down point; a smaller knob follows
the finger within a maximum radius. When the finger lifts, both
disappear.

- **Dead zone:** 15% of max radius (configurable)
- **Max radius:** 56dp (scales with control size setting)
- **Visual:**
  - Outer ring: white, 25% opacity, 2dp stroke
  - Base disc: white, 12% opacity, filled
  - Inner knob: white, 50% opacity, filled, with subtle drop shadow
  - Direction indicators: four faint arrows at compass points
    (8% opacity), highlight to 40% when that direction is active
- **Output:** Normalized direction fed to `ATInputManager` as digital
  joystick (4-way or 8-way, matching Atari hardware)
- **Idle hint:** When no finger is in the joystick zone, a faint
  circle (8% opacity) with a small crosshair shows the default
  center position, signaling "touch here to control"

Implementation:

```cpp
// On FINGER_DOWN in left zone:
joy_active = true;
base = touch_pos;          // joystick appears here

// On FINGER_MOTION:
delta = touch_pos - base;
if (length(delta) > dead_zone) {
    // Convert to 4/8 directional mask
    uint32_t dirs = AnalogToDirectionMask(delta);
    // Feed to input manager as joystick directions
    g_inputManager.OnDigitalInput(kATInputCode_JoyLeft,  dirs & kDir_Left);
    g_inputManager.OnDigitalInput(kATInputCode_JoyRight, dirs & kDir_Right);
    g_inputManager.OnDigitalInput(kATInputCode_JoyUp,    dirs & kDir_Up);
    g_inputManager.OnDigitalInput(kATInputCode_JoyDown,  dirs & kDir_Down);
}

// On FINGER_UP:
joy_active = false;
// Release all directions
```

### Virtual D-Pad (alternative, selectable in settings)

A fixed-position cross rendered in the left zone. Four directional
hit areas (up/down/left/right) with optional diagonal corners for
8-way input. Always visible (does not follow the thumb).

- **Visual:** Four arrow segments in a cross pattern, highlights on touch
- **Hit areas:** Pie-slice sectors centered on each cardinal direction
- **Diagonal:** Corner sectors between cardinals (8-way mode)
- **Size:** Cross fits within a 112dp square; each arm is 48dp wide

### Fire Buttons

Two circular buttons stacked vertically in the right zone:

- **FIRE A** (primary, lower) -- maps to Joystick Trigger (TRIG0)
  - Size: **56dp** diameter (circular)
  - Color: red-tinted (rgba 180, 50, 50, 0.65)
  - Label: "A" in bold, 18dp font
- **FIRE B** (secondary, upper) -- maps to second button (5200 mode)
  or user-configurable action
  - Size: **48dp** diameter (circular, slightly smaller than A)
  - Color: blue-tinted (rgba 50, 70, 180, 0.65)
  - Label: "B" in bold, 16dp font
- **Spacing:** 16dp gap between A and B
- **Hit area:** Each button's tappable circle extends 4dp beyond its
  visual edge (invisible padding)

Visual feedback on press:
- Scale to 90% over 50ms (spring animation)
- Brightness increases (alpha goes from 0.65 to 0.85)
- If haptic is enabled, trigger a short 15ms vibration pulse

### Console Keys (START / SELECT / OPTION)

Three pill-shaped buttons in the top bar, left-aligned:

- **START** -- POKEY CONSOL bit 0 (active low)
- **SELECT** -- POKEY CONSOL bit 1
- **OPTION** -- POKEY CONSOL bit 2

These feed directly into the console switch register via the existing
`ATSimulator::SetConsoleSwitch()` path, the same as keyboard F2/F3/F4
on desktop.

Sizing:
- **Width:** 72dp (enough for the label text with 16dp horizontal padding)
- **Height:** 36dp visual, **48dp** tappable (6dp invisible padding
  top and bottom)
- **Spacing:** 8dp between buttons
- **Corner radius:** height / 2 (fully rounded ends = pill shape)

Visual:
- Background: dark blue-gray (rgba 50, 50, 70, 0.55)
- Text: white, 13dp font, centered
- Border: 1dp white at 15% opacity
- On press: background brightens to 0.75 alpha, text to full white
- These buttons are intentionally less prominent than fire buttons --
  they're used occasionally, not continuously

### Menu Button (Hamburger Icon)

Top-right corner of the top bar:

- **Size:** 48dp x 48dp tappable area
- **Icon:** Three horizontal lines, 20dp wide, 2dp thick, 5dp spacing
  - Color: white at 70% opacity
- **On press:** Icon brightens to 90% opacity
- The visual icon is smaller than the hit area -- the 48dp tap zone
  extends around it

### Haptic Feedback

All button presses trigger a short haptic pulse:

- **Fire buttons:** 15ms pulse (short, crisp click)
- **Console keys:** 10ms pulse (lighter)
- **Joystick direction change:** 5ms pulse (subtle tick)

Implementation priority:
1. `SDL_RumbleGamepad()` if a gamepad is connected
2. Android Vibrator service via JNI: `android.os.Vibrator.vibrate()`
3. SDL3 haptic subsystem as fallback

Haptic feedback is optional, toggled in settings. Default: on.

### Multi-Touch

Each control zone tracks its own `SDL_FingerID`:

- Left zone: joystick/dpad finger
- Right zone: fire button finger(s) -- both can be held simultaneously
- Top bar: console key finger

Fingers are independent. Pressing START while holding fire and
joystick works correctly. A finger that starts in one zone stays
assigned to that zone until lifted, even if it drifts outside.

## Hamburger Menu

Tapping the menu icon (top-right) pauses emulation and slides in
a panel from the right edge.

### Layout

```
+---------------------------------------+
|                  |                     |
|                  |  Altirra       [X]  |
|   (dimmed        |---------------------|
|    game           |                     |
|    display,       |  ▶  Resume          |
|    tappable       |                     |
|    to close)      |  📂 Load Game       |
|                  |                     |
|                  |  💾 Disk Drives      |
|                  |                     |
|                  |  🔊 Audio: ON        |
|                  |                     |
|                  |  ─────────────────  |
|                  |                     |
|                  |  🔄 Warm Reset       |
|                  |                     |
|                  |  ⚡ Cold Reset       |
|                  |                     |
|                  |  ─────────────────  |
|                  |                     |
|                  |  ⌨  Virtual Keyboard |
|                  |                     |
|                  |  ⚙  Settings         |
|                  |                     |
|                  |  ─────────────────  |
|                  |                     |
|                  |  ℹ  About            |
|                  |                     |
+---------------------------------------+
```

### Panel Sizing

- **Width:** 70% of screen width on phones (< 600dp wide),
  50% on tablets (>= 600dp). Minimum 280dp, maximum 400dp.
- **Height:** full screen height
- **Background:** solid dark (rgba 25, 25, 30, 0.97) -- nearly opaque
  for readability
- **Dim overlay:** the area left of the panel gets a 50% black overlay

### Slide-In Animation

The panel slides in from the right edge over 200ms with an ease-out
curve (starts fast, decelerates). The dim overlay fades in over the
same duration. Closing reverses the animation (slide out + fade out).

```cpp
// Animation state
float menuSlide = 0.0f;  // 0.0 = closed, 1.0 = fully open
bool  menuOpening = true;

// Each frame while animating:
float target = menuOpening ? 1.0f : 0.0f;
float speed = 1.0f / 0.2f;  // 200ms
menuSlide = MoveTowards(menuSlide, target, speed * deltaTime);

// Panel X position (slides from off-screen right to final position)
float panelX = Lerp(screenW, screenW - menuW, EaseOut(menuSlide));
```

### Menu Items

Each menu item is a full-width row:

- **Height:** 56dp
- **Left padding:** 20dp
- **Icon:** Simple geometric shape drawn with ImGui (not Unicode emoji),
  16dp square, left-aligned
- **Label:** 16dp font, 12dp after icon
- **Tap highlight:** On touch-down, the row background brightens to
  rgba(255, 255, 255, 0.08) for 100ms
- **Separator lines:** 1dp, rgba(255, 255, 255, 0.10), full width

| Item | Icon | Action |
|------|------|--------|
| **Resume** | ▶ triangle | Close menu, unpause emulation |
| **Load Game** | Folder outline | Open file browser (see below) |
| **Disk Drives** | Disk outline | Open disk drive management panel (mount/eject/swap for D1:-D4:) |
| **Audio: ON/OFF** | Speaker / Muted speaker | Toggle mute, visual indicator updates |
| **Warm Reset** | Circular arrow | `ATSimulator::WarmReset()`, close menu, resume |
| **Cold Reset** | Power symbol | `ATSimulator::ColdReset()`, close menu, resume |
| **Virtual Keyboard** | Keyboard outline | Show on-screen Atari keyboard overlay (Phase 2) |
| **Settings** | Gear | Open settings panel |
| **About** | Info circle | Version, credits, license |

Icons are drawn programmatically using `ImDrawList` primitives (lines,
circles, triangles). This avoids font atlas complexity and gives precise
control over size and color. Each icon is 16x16dp, drawn in white at
60% opacity.

### Close Button

The title bar has an **X** button (48dp x 48dp hit area) in the
top-right corner of the panel. It also closes the menu.

Emulation is paused while the menu is open. Tapping outside the menu
panel, pressing the device back button, or tapping [X] closes it
and resumes.

## File Browser

Opened from the hamburger menu ("Load Game") or shown on the main screen
if no image is loaded (centered "Load Game" button over the display area).

### Layout

```
+------------------------------------------+
|  [←]          Load Game             [X]  |   <- 56dp header bar
|------------------------------------------|
|                                          |
|  RECENT                                  |   <- section header, 13dp
|    ┌──┐                                  |      uppercase, muted color
|    │XE│  Star Raiders.xex                |
|    └──┘  /storage/.../Atari/             |   <- 72dp rows with
|    ┌──┐                                  |      file icon + subtext
|    │AT│  Mule.atr                        |
|    └──┘  /storage/.../Downloads/         |
|    ┌──┐                                  |
|    │CA│  Jumpman.car                     |
|    └──┘  /storage/.../Games/             |
|                                          |
|  ─────────────────────────────────────── |
|                                          |
|  BROWSE                                  |   <- section header
|  /storage/emulated/0/Downloads           |   <- path, 12dp, muted
|                                          |
|    📁 Atari                              |   <- 56dp rows
|    📁 Games                              |
|    📄 game1.xex                          |
|    📄 game2.atr                          |
|    📄 disk_collection.zip                |
|                                          |
|  ─────────────────────────────────────── |
|  [Internal Storage]    [SD Card]         |   <- 48dp pill buttons
+------------------------------------------+
```

### Row Sizing

- **Recent file rows:** 72dp tall
  - Top line: filename, 15dp font, white
  - Bottom line: directory path (truncated from left), 12dp font,
    50% opacity
  - Left: file type badge (colored rounded rect, 32x32dp, with
    2-3 letter extension abbreviation)
- **Browse directory rows:** 56dp tall
  - Icon: folder (filled) or file (outline), 24dp, left-aligned
  - Label: filename, 15dp font
- **Header button bar:** 56dp
  - Back arrow [←]: 48dp hit area, 24dp arrow icon
  - Title centered
  - Close [X]: 48dp hit area

### File Type Badges (Recent Files)

Each recent file shows a colored badge with the file extension:

| Extension | Badge color | Label |
|-----------|-------------|-------|
| .xex, .obx, .com | Green (60, 160, 60) | XEX |
| .atr, .xfd, .atx | Blue (60, 100, 200) | ATR |
| .car | Orange (200, 140, 40) | CAR |
| .cas | Purple (140, 60, 180) | CAS |
| .rom, .bin | Gray (120, 120, 120) | ROM |
| .zip, .gz | Yellow (180, 170, 40) | ZIP |

### Behavior

- **File types:** `.xex`, `.atr`, `.car`, `.bin`, `.rom`, `.cas`, `.dcm`,
  `.atz`, `.zip`, `.gz`, `.xfd`, `.atx`, `.obx`, `.com`, `.exe`
  (auto-extract single-file archives)
- **Recent files:** Last 20 loaded images, stored in settings. Shown
  at the top of the file browser. Tap to load immediately without
  navigating.
- **Default directory:** `Downloads` on first boot, user-configurable
- **Directory navigation:** Tap folder to enter. Back arrow [←] goes
  up one level. Swipe right from left edge also navigates up.
- **Storage roots:** Pill-shaped buttons at the bottom to jump to
  internal storage or SD card
- **Android permissions:** Uses Storage Access Framework (SAF) on
  Android 11+ via `SDL_ShowOpenFileDialog()` or JNI to
  `Intent.ACTION_OPEN_DOCUMENT` as fallback
- **Scroll inertia:** The file list uses ImGui's built-in touch
  scrolling with momentum

### ROM/Firmware Discovery

On first boot (no firmware found), the file browser is shown with a
message: "Altirra needs Atari firmware ROMs to run. Select a folder
containing ROM files, or use the built-in replacement kernel."

Firmware search is recursive within the selected directory. Known
firmware is identified by CRC32 match against the existing firmware
database in `ATFirmwareManager`. The user can also use the built-in
HLE kernel (no external ROMs required for basic functionality).

## Settings Screen

Full-screen panel replacing the hamburger menu when opened.

### Layout

```
+------------------------------------------+
|  [←]          Settings              [X]  |   <- 56dp header bar
|------------------------------------------|
|                                          |
|  SYSTEM                                  |   <- section header
|                                          |
|  Video Standard                          |
|  [ NTSC ][ PAL ]                         |   <- segmented control
|                                          |
|  Memory Size                             |
|  [ 16K ][ 48K ][ 64K ][ 128K ][▼ More]  |   <- segmented + overflow
|                                          |
|  BASIC                                   |
|  ───────────────────────────── [  ○    ] |   <- toggle switch
|                                          |
|  SIO Patch                               |
|  ───────────────────────────── [  ● ━━━] |   <- toggle switch (on)
|                                          |
|  ─────────────────────────────────────── |
|                                          |
|  CONTROLS                                |
|                                          |
|  Input Style                             |
|  [ Joystick ][ D-Pad ]                   |   <- segmented control
|                                          |
|  Control Size                            |
|  [ S ][ M ][ L ]                         |   <- segmented control
|                                          |
|  Opacity                                 |
|  ●━━━━━━━━━━━━━━━━━━━━━○────  50%        |   <- slider
|                                          |
|  Haptic Feedback                         |
|  ───────────────────────────── [  ● ━━━] |   <- toggle switch
|                                          |
|  ─────────────────────────────────────── |
|                                          |
|  DISPLAY                                 |
|                                          |
|  Filter Mode                             |
|  [ Sharp ][ Bilinear ][ Sharp Bilinear ] |
|                                          |
|  Stretch Mode                            |
|  [ Aspect ][ Fill ][ Integer ]           |
|                                          |
|  Show FPS                                |
|  ───────────────────────────── [  ○    ] |
|                                          |
|  ─────────────────────────────────────── |
|                                          |
|  AUDIO                                   |
|                                          |
|  Volume                                  |
|  ●━━━━━━━━━━━━━━━━━━━━━━━━━━○  80%      |
|                                          |
|  ─────────────────────────────────────── |
|                                          |
|  FIRMWARE                                |
|                                          |
|  ROM Directory                           |
|  /storage/.../Atari/ROMs    [Change]     |
|  Status: 3 ROMs found ✓                  |
|                                          |
+------------------------------------------+
```

### Widget Sizing for Touch

Standard ImGui widgets (Checkbox, Combo, SliderFloat) are designed for
mouse interaction and are too small for fingers. The mobile settings
screen uses **custom-drawn touch widgets** instead:

**Toggle Switch** (replaces Checkbox):
- Track: 52dp wide, 28dp tall, fully rounded
- Thumb: 24dp circle
- Off state: track is dark gray, thumb is on the left
- On state: track is accent color (blue), thumb slides to the right
- Hit area: full row width, 56dp tall (tap anywhere on the row)
- Animation: thumb slides over 100ms with ease-out

**Segmented Control** (replaces Combo for 2-5 options):
- Each segment: minimum 56dp wide, 40dp tall
- Active segment: filled with accent color, white text
- Inactive segments: outlined, muted text
- Corner radius: 8dp (outer) / 6dp (inner segments)
- The segmented control is preferred over dropdown menus because
  it shows all options at a glance and requires a single tap

**Slider** (replaces ImGui SliderFloat):
- Track: full available width - 80dp (leaving room for value label),
  4dp tall
- Thumb: 24dp diameter circle, with a 48dp invisible hit area
- Active track (left of thumb): accent color
- Inactive track (right of thumb): dark gray
- Value label: right-aligned, 14dp font, e.g. "50%"
- Drag behavior: finger can touch anywhere on the track to jump,
  then drag. Finger doesn't need to land exactly on the thumb.

**Section Header:**
- Text: 12dp, uppercase, accent color (muted blue)
- Top margin: 24dp (except first section)
- Bottom margin: 8dp

**Setting Row:**
- Label: 15dp font, white
- Height: 56dp minimum (toggle rows) or auto (segmented controls
  add 40dp for the control below the label)
- Left/right padding: 20dp

### Setting Details

| Setting | Widget | Values | Maps to |
|---------|--------|--------|---------|
| Video Standard | Segmented | PAL / NTSC | `ATSimulator::SetVideoStandard()` |
| Memory Size | Segmented + overflow | 16K / 48K / 64K / 128K / 320K / 1088K | `ATSimulator::SetMemoryMode()` |
| BASIC | Toggle | On / Off | `ATSimulator::SetBASICEnabled()` |
| SIO Patch | Toggle | On / Off | `ATSimulator::SetSIOPatchEnabled()` |
| Input Style | Segmented | Joystick / D-Pad | Touch controls layout selector |
| Control Size | Segmented | S / M / L | Scales touch control zones |
| Opacity | Slider | 10%--100% | Alpha of touch overlays |
| Haptic Feedback | Toggle | On / Off | Enable/disable vibration |
| Filter Mode | Segmented | Sharp / Bilinear / Sharp Bilinear | Display texture filtering |
| Stretch Mode | Segmented | Aspect / Fill / Integer | Display scaling mode |
| Show FPS | Toggle | On / Off | FPS counter overlay |
| Volume | Slider | 0%--100% | `ATSimulator` audio volume |
| ROM Directory | Button row | Directory picker | Firmware search root |

Settings persist to `~/.config/altirra/settings.ini` (same file as
desktop, Android path resolved via `SDL_GetPrefPath()`).

### Back Navigation

The back arrow [←] in the header (48dp hit area) returns to the
hamburger menu. The Android system back gesture / back button also
closes settings and returns to the menu. Pressing back from the menu
closes it and resumes emulation.

Navigation stack: Settings → Menu → Game. Each back action pops one
level.

## First-Boot Experience

1. Splash screen with Altirra logo (brief, <1s)
2. "Welcome to Altirra" screen:
   - Title: "Altirra" in 24dp font
   - Subtitle: "Atari 800/XL/5200 Emulator" in 14dp, muted
   - Body: "To get started, select a folder containing Atari ROM
     firmware, or tap Skip to use the built-in replacement kernel."
     in 15dp, 80% opacity
   - Two buttons, centered, stacked vertically, 56dp tall, 240dp wide:
     - [Select ROM Folder] — accent color, filled
     - [Skip — Use Built-in Kernel] — outlined, muted
3. If ROMs found: show file browser to load a game
4. If skipped: show file browser to load a game (HLE kernel active)
5. If no game loaded: main screen with "Load Game" button centered
   (56dp tall, 200dp wide, accent color, with a subtle pulse animation
   to draw attention)

On subsequent boots, go straight to the last-loaded game (if any)
or the "Load Game" screen.

## Virtual Keyboard (Phase 2)

A full Atari keyboard overlay covering the bottom ~50% of the screen.
Layout matches the Atari 800XL keyboard: 4 rows, QWERTY layout, with
special keys (BREAK, CAPS, HELP, INVERSE, CLEAR).

```
+--------------------------------------------------+
|  Esc 1 2 3 4 5 6 7 8 9 0 < > BS                 |
|  Tab  Q W E R T Y U I O P - = Ret               |
|  Ctrl  A S D F G H J K L ; + *                   |
|  Shift  Z X C V B N M , . / Shift               |
|       [HELP] [     SPACE     ] [CAPS] [BRK]     |
+--------------------------------------------------+
```

Key sizing:
- Standard key: 40dp wide, 44dp tall (slightly below 48dp minimum
  for density, but matches real keyboard UX conventions)
- Space bar: 160dp wide
- Modifier keys (Shift, Ctrl): 56dp wide

Each key press sends the corresponding Atari scancode through the
existing POKEY keyboard path (`ATInputSDL3_PushRawKey()`).

The keyboard slides up from the bottom with animation (200ms ease-out).
While visible, the emulator display scales down to fit above it. A
"hide keyboard" button (or swipe-down gesture) dismisses it.

This is a Phase 2 feature due to complexity (key repeat, shift states,
CTRL combinations, layout for multiple Atari models).

## Architecture

### New Files

| File | Purpose |
|------|---------|
| `src/AltirraSDL/source/ui_mobile.h` | Mobile UI state, hamburger menu, settings panel |
| `src/AltirraSDL/source/ui_mobile.cpp` | Mobile UI rendering and input handling |
| `src/AltirraSDL/source/touch_controls.h` | Touch control definitions and state |
| `src/AltirraSDL/source/touch_controls.cpp` | Touch control rendering, hit testing, input routing |
| `src/AltirraSDL/source/touch_layout.h` | Adaptive layout engine (zone calculation, dp conversion) |
| `src/AltirraSDL/source/touch_layout.cpp` | Portrait/landscape layout computation, safe area |
| `src/AltirraSDL/source/touch_widgets.h` | Touch-friendly custom ImGui widgets (toggle, segmented, slider) |
| `src/AltirraSDL/source/touch_widgets.cpp` | Widget rendering and interaction logic |
| `src/AltirraSDL/source/file_browser_mobile.h` | Mobile file browser UI |
| `src/AltirraSDL/source/file_browser_mobile.cpp` | Directory scanning, recent files, SAF integration |

### Modified Files

| File | Change |
|------|--------|
| `main_sdl3.cpp` | `#ifdef __ANDROID__` blocks for mobile init, touch event routing, safe area query |
| `CMakeLists.txt` | Android toolchain detection, mobile source files, gradle integration |

### Build System

The Android build uses the SDL3 android-project template:

```
android-project/
  app/
    build.gradle
    src/main/
      AndroidManifest.xml
      java/       <- SDL3 Android activity (from SDL3 source)
      jni/
        CMakeLists.txt  <- includes our top-level CMakeLists.txt
  gradle/
  build.gradle
  settings.gradle
```

SDL3 provides `SDL_android.c` and the Java activity
(`SDLActivity.java`) that handles the Android lifecycle and creates
the GL surface. Our CMakeLists.txt compiles into a shared library
(`libmain.so`) that SDL3's activity loads.

The existing CMakeLists.txt already has `if(ANDROID)` detection.
The Android build adds:

```cmake
if(ANDROID)
    target_sources(AltirraSDL PRIVATE
        source/ui_mobile.cpp
        source/touch_controls.cpp
        source/touch_layout.cpp
        source/touch_widgets.cpp
        source/file_browser_mobile.cpp
    )
    target_compile_definitions(AltirraSDL PRIVATE ALTIRRA_MOBILE=1)
endif()
```

Desktop builds do not compile or link any mobile UI code.

### Input Routing

Touch events integrate with the existing input infrastructure:

```
SDL_EVENT_FINGER_DOWN / MOTION / UP
    |
    v
touch_controls.cpp (zone assignment, gesture recognition)
    |
    +-- Joystick/D-Pad direction --> ATInputManager::OnDigitalInput()
    |                                (kATInputCode_Joy*)
    |
    +-- Fire buttons --> ATInputManager::OnDigitalInput()
    |                    (kATInputCode_JoyButton0/1)
    |
    +-- Console keys --> ATSimulator::SetConsoleSwitch()
    |                    (START/SELECT/OPTION bits)
    |
    +-- Virtual keyboard --> ATInputSDL3_PushRawKey()
    |                        (POKEY scancode path)
    |
    +-- UI elements --> ImGui (hamburger, file browser, settings)
```

Touch events that land in UI zones (hamburger menu, file browser) are
forwarded to ImGui via `ImGui_ImplSDL3_ProcessEvent()`. Touch events
in control zones are consumed by `touch_controls.cpp` and never reach
ImGui.

### State Management

The mobile UI introduces a simple state machine:

```
BOOT --> FIRST_RUN_WIZARD --> FILE_BROWSER --> RUNNING
                                  ^               |
                                  |               v
                              HAMBURGER <---> PAUSED
                                  |
                                  v
                              SETTINGS
```

- **RUNNING:** Emulation active, touch controls visible, top bar auto-hides
- **PAUSED:** Emulation paused (hamburger open, or explicit pause)
- **FILE_BROWSER:** Full-screen file browser, emulation paused
- **SETTINGS:** Full-screen settings, emulation paused
- **FIRST_RUN_WIZARD:** One-time firmware setup

State transitions are explicit. Every state that pauses emulation
resumes it on exit (or returns to the previous state).

Back button / back gesture navigation:
```
SETTINGS    → back → HAMBURGER
FILE_BROWSER → back → HAMBURGER
HAMBURGER   → back → RUNNING (resume)
RUNNING     → back → (Android default: minimize app)
```

## Orientation Handling

SDL3 on Android receives `SDL_EVENT_WINDOW_RESIZED` on rotation.
The layout engine recalculates all zones on every resize:

```cpp
void TouchLayout_Update(int screen_w, int screen_h, float contentScale,
                        const SafeInsets &insets)
{
    bool landscape = (screen_w > screen_h);

    // Convert safe insets to normalized coordinates
    float safeL = insets.left / screen_w;
    float safeR = insets.right / screen_w;
    float safeT = insets.top / screen_h;
    float safeB = insets.bottom / screen_h;

    // dp-to-pixel conversion
    auto dp = [&](float v) { return v * contentScale; };

    // Top bar height: 56dp
    float topBarH = dp(56) / screen_h;
    float topBarTop = safeT;  // starts below safe inset

    if (landscape) {
        joy_zone   = { safeL, topBarTop + topBarH, 0.30f, 1.0f - safeB };
        fire_zone  = { 0.78f, 0.45f, 1.0f - safeR, 1.0f - safeB };
        top_bar    = { safeL, topBarTop, 1.0f - safeR, topBarTop + topBarH };
        display    = { 0.12f, topBarTop + topBarH, 0.80f, 1.0f - safeB };
    } else {
        float ctrlTop = 0.68f;
        joy_zone   = { safeL, ctrlTop, 0.50f, 1.0f - safeB };
        fire_zone  = { 0.62f, ctrlTop, 1.0f - safeR, 1.0f - safeB };
        top_bar    = { safeL, topBarTop, 1.0f - safeR, topBarTop + topBarH };
        display    = { safeL, topBarTop + topBarH, 1.0f - safeR, ctrlTop };
    }
}
```

The Android manifest declares support for all orientations:

```xml
<activity
    android:screenOrientation="fullSensor"
    android:configChanges="orientation|screenSize|screenLayout|keyboardHidden">
```

SDL3 hint for allowed orientations:

```cpp
SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight Portrait PortraitUpsideDown");
```

## Android-Specific Considerations

### Permissions

- **Storage:** `READ_EXTERNAL_STORAGE` (API < 33) or
  `READ_MEDIA_IMAGES` + SAF for API 33+. ROM files and disk images
  must be readable.
- **Vibration:** `android.permission.VIBRATE` for haptic feedback.
- No network permissions needed (emulated network is internal).

### Performance

- OpenGL ES 3.0 is the minimum (maps well to our GL 3.3 backend with
  minor shader adjustments: `#version 300 es`, precision qualifiers).
- The emulation core is CPU-bound and single-threaded. Modern phones
  have more than enough single-core performance for 6502 emulation at
  1.79 MHz.
- Frame pacing: Use `SDL_SetSwapInterval(1)` for vsync. The emulator's
  existing frame pacing logic handles the rest.

### Lifecycle

Android can pause/resume/destroy the activity at any time. SDL3
translates these to `SDL_EVENT_DID_ENTER_BACKGROUND` and
`SDL_EVENT_WILL_ENTER_FOREGROUND`.

On background:
- Pause emulation
- Mute audio
- Release all touch controls (`ATTouchControls_ReleaseAll()`)
- Save state snapshot (optional, for instant resume)

On foreground:
- Restore state
- Resume audio
- Resume emulation (or stay paused if user paused manually)

On destroy:
- Save settings
- Flush any pending state

### Audio

SDL3 audio works on Android without changes. The existing
`ATAudioOutputSDL3` implementation handles device open/close and
callback-based mixing. Android audio latency varies by device; the
existing buffer size negotiation should work.

### App Icon and Metadata

The APK needs:
- App icon (Altirra logo, adaptive icon format for Android 8+)
- App name: "Altirra"
- Min SDK: 24 (Android 7.0 -- covers 99%+ of active devices)
- Target SDK: 34 (current requirement for Play Store)

## Visual Design Language

### Color Palette

The mobile overlay uses a minimal, dark color scheme that doesn't
distract from the game display:

| Element | Color (RGBA) | Notes |
|---------|-------------|-------|
| Control background | (50, 50, 60, 0.55) | Semi-transparent dark |
| Control pressed | (70, 70, 85, 0.75) | Brightens on touch |
| Fire A | (180, 50, 50, 0.65) | Red, primary action |
| Fire B | (50, 70, 180, 0.65) | Blue, secondary action |
| Accent / active | (70, 130, 230, 1.0) | Toggle on, segment active |
| Text primary | (255, 255, 255, 0.90) | Labels, headings |
| Text secondary | (255, 255, 255, 0.50) | Subtitles, paths |
| Separator | (255, 255, 255, 0.10) | Divider lines |
| Menu background | (25, 25, 30, 0.97) | Nearly opaque |
| Overlay dim | (0, 0, 0, 0.50) | Behind menu panels |
| Joystick ring | (255, 255, 255, 0.25) | Outer boundary |
| Joystick knob | (255, 255, 255, 0.50) | Follows finger |
| Joystick idle hint | (255, 255, 255, 0.08) | Ghost circle |

### Typography

ImGui's default font is too small on mobile. The mobile UI loads the
font at multiple sizes scaled by `contentScale`:

| Use | Base size | Actual (at 2.5x) |
|-----|-----------|-------------------|
| Body / labels | 15dp | 37.5px |
| Section headers | 12dp | 30px |
| Button labels (fire) | 18dp | 45px |
| Title | 20dp | 50px |
| Small / path text | 12dp | 30px |

Font loading at startup:

```cpp
float cs = SDL_GetDisplayContentScale(display);
ImGuiIO &io = ImGui::GetIO();
io.Fonts->AddFontDefault();  // fallback
// Load primary font at mobile-appropriate sizes
// (or use the default font with scaled global size)
io.FontGlobalScale = cs;  // simple approach: scale everything
```

The `FontGlobalScale` approach is simplest but makes all ImGui text
larger, including desktop UI if compiled without `ALTIRRA_MOBILE`. The
alternative is to load the font at the desired pixel size and use it
explicitly.

### Drop Shadows

Interactive elements (fire buttons, joystick knob, menu panel) have
a subtle drop shadow for visual depth:

```cpp
// Shadow: offset (2dp, 2dp), blur approximated by drawing a larger
// shape behind at lower opacity
float shadowOff = dp(2);
ImU32 shadowColor = IM_COL32(0, 0, 0, 60);
dl->AddCircleFilled(ImVec2(cx + shadowOff, cy + shadowOff),
    radius + dp(1), shadowColor, 32);
// Then draw the actual button on top
```

Shadows are subtle (not realistic) -- just enough to lift elements
off the game display and clarify that they're interactive.

### Press Animations

All interactive elements respond visually to touch:

| Element | Press effect | Duration |
|---------|-------------|----------|
| Fire buttons | Scale to 92%, alpha +0.20 | 50ms ease-out |
| Console keys | Alpha +0.20, background brighten | 50ms |
| Menu items | Row background highlight | 100ms |
| Toggle switch | Thumb slides, track color changes | 100ms ease-out |
| Segmented control | Active segment color fill | 80ms |

The scale animation for fire buttons is applied by adjusting the
button rect before drawing:

```cpp
float pressScale = Lerp(1.0f, 0.92f, pressAmount);  // pressAmount 0..1
ATTouchRect scaled = ScaleFromCenter(btnRect, pressScale);
DrawButton(dl, scaled, ...);
```

## Implementation Phases

### Phase 1: Foundation

- Android build system (gradle + CMake, APK packaging)
- DPI-aware layout engine (dp conversion, `SDL_GetDisplayContentScale`)
- Safe area query and inset handling
- Basic touch input routing (joystick + fire -> ATInputManager)
- Adaptive layout engine (portrait/landscape zone calculation)
- Touch control rendering (ImGui overlay with proper sizing)
- Console key buttons (START/SELECT/OPTION)
- Hamburger menu with slide-in animation
- Font scaling for mobile DPI

### Phase 2: Usability

- File browser with directory navigation and recent files
- ROM/firmware discovery (recursive scan, CRC matching)
- First-boot wizard
- Settings screen with touch-friendly widgets (toggle, segmented, slider)
- Audio mute toggle (wired to actual audio)
- Haptic feedback
- Top bar auto-hide with fade animation
- Android back button / gesture navigation
- Press animations on all interactive elements
- SIO Patch, Stretch Mode, Show FPS, Volume settings

### Phase 3: Polish

- Virtual D-Pad alternative
- Control size/opacity customization with live preview
- Virtual Atari keyboard overlay
- Android lifecycle handling (background/foreground)
- State save/restore on backgrounding
- App icon, Play Store metadata
- Drop shadows and visual refinement
- Menu slide-in / slide-out animation

### Phase 4: Advanced (future)

- Save states (quick save/load from hamburger)
- Shader effects (CRT filter via librashader, if GPU allows)
- Bluetooth gamepad support (already works via SDL3 gamepad API)
- Multiplayer (second player via gamepad)
- Tape control (cassette transport)
- 5200 controller mode (analog joystick + numeric keypad overlay)

## Comparison to ANDROID.md Example

The `NEW-ANDROID/ANDROID.md` example demonstrates basic SDL3 touch
principles (multi-touch finger tracking, normalized coordinates,
orientation detection) but is not suitable as an implementation base:

| Aspect | Example | Required |
|--------|---------|----------|
| Architecture | Standalone demo | Integrated into AltirraSDL |
| Controls | 2 buttons + joystick | START/SELECT/OPTION + 2 fire + joystick/dpad |
| Menu system | None | Hamburger with slide-in panel |
| Configuration | None | PAL/NTSC, memory, controls, firmware |
| File loading | None | File browser with SAF, recent files |
| Rendering | SDL_RenderFillRect | ImGui overlay on GL display backend |
| Input routing | Direct float vars | ATInputManager + POKEY keyboard |
| Haptics | None | SDL3 haptic / Android vibrator |
| Layout | Hardcoded percentages | Adaptive zone engine with dp + safe area |
| State management | None | Pause/resume/lifecycle/settings persistence |
| DPI awareness | None | Full dp-based sizing via content scale |
| Safe area | None | Notch/cutout/nav bar inset handling |

The example's finger-tracking pattern (per-zone `SDL_FingerID`
assignment) is a valid technique and should be reused in
`touch_controls.cpp`. The rest should be built from scratch on top
of the existing AltirraSDL infrastructure.
