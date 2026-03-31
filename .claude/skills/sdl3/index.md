# SDL3 API Index

Quick reference to find the right category. Read the category file for the full function list.

## Basics

- **Main** (17 symbols) — Redefine main() if necessary so that it is called by SDL. In order to make this consistent on all platforms, the application's main() should look like
  File: `docs/SDL3/CategoryMain.md`
- **Init** (17 symbols) — All SDL programs need to initialize the library before starting to work with it. Almost everything can simply call [SDL_Init](SDL_Init)() near startup
  File: `docs/SDL3/CategoryInit.md`
- **Hints** (277 symbols) — This file contains functions to set and get configuration hints, as well as listing each of them alphabetically. The convention for naming hints is [S
  File: `docs/SDL3/CategoryHints.md`
- **Properties** (31 symbols) — A property is a variable that can be created and retrieved by name at runtime. All properties are part of a property group ([SDL_PropertiesID](SDL_Pro
  File: `docs/SDL3/CategoryProperties.md`
- **Error** (7 symbols) — Simple error message routines for SDL. Most apps will interface with these APIs in exactly one function: when almost any SDL function call reports fai
  File: `docs/SDL3/CategoryError.md`
- **Log** (21 symbols) — Simple log messages with priorities and categories. A message's [SDL_LogPriority](SDL_LogPriority) signifies how important the message is. A message's
  File: `docs/SDL3/CategoryLog.md`
- **Assert** (23 symbols) — A helpful assertion macro! SDL assertions operate like your usual `assert` macro, but with some added features: - It uses a trick with the `sizeof` op
  File: `docs/SDL3/CategoryAssert.md`
- **Version** (12 symbols) — Functionality to query the current SDL version, both as headers the app was compiled against, and a library the app is linked to.
  File: `docs/SDL3/CategoryVersion.md`

## Video

- **Video** (148 symbols) — SDL's video subsystem is largely interested in abstracting window management from the underlying operating system. You can create windows, manage them
  File: `docs/SDL3/CategoryVideo.md`
- **Render** (118 symbols) — Header file for SDL 2D rendering functions. This API supports the following features: - single pixel points - single pixel lines - filled rectangles -
  File: `docs/SDL3/CategoryRender.md`
- **Pixels** (61 symbols) — SDL offers facilities for pixel management. Largely these facilities deal with pixel *format*: what does this set of bits represent? If you mostly wan
  File: `docs/SDL3/CategoryPixels.md`
- **Blendmode** (4 symbols) — Blend modes decide how two colors will mix together. There are both standard modes for basic needs and a means to create custom modes, dictating what 
  File: `docs/SDL3/CategoryBlendmode.md`
- **Rect** (22 symbols) — Some helper functions for managing rectangles and 2D points, in both integer and floating point versions.
  File: `docs/SDL3/CategoryRect.md`
- **Surface** (70 symbols) — SDL surfaces are buffers of pixels in system RAM. These are useful for passing around and manipulating images that are not stored in GPU memory. [SDL_
  File: `docs/SDL3/CategorySurface.md`
- **Clipboard** (13 symbols) — SDL provides access to the system clipboard, both for reading information from other processes and publishing information of its own. This is not just
  File: `docs/SDL3/CategoryClipboard.md`
- **Vulkan** (7 symbols) — Functions for creating Vulkan surfaces on SDL windows. For the most part, Vulkan operates independent of SDL, but it benefits from a little support du
  File: `docs/SDL3/CategoryVulkan.md`
- **Metal** (4 symbols) — Functions to creating Metal layers and views on SDL windows. This provides some platform-specific glue for Apple platforms. Most macOS and iOS apps ca
  File: `docs/SDL3/CategoryMetal.md`
- **Camera** (20 symbols) — Video capture for the SDL library. This API lets apps read input from video sources, like webcams. Camera devices can be enumerated, queried, and open
  File: `docs/SDL3/CategoryCamera.md`

## Input Events

- **Events** (62 symbols) — Event queue management. It's extremely common--often required--that an app deal with SDL's event queue. Almost all useful information about interactio
  File: `docs/SDL3/CategoryEvents.md`
- **Keyboard** (27 symbols) — SDL keyboard management. Please refer to the Best Keyboard Practices document for details on how best to accept keyboard input in various types of pro
  File: `docs/SDL3/CategoryKeyboard.md`
- **Keycode** (2 symbols) — Defines constants which identify keyboard keys and modifiers. Please refer to the Best Keyboard Practices document for details on what this informatio
  File: `docs/SDL3/CategoryKeycode.md`
- **Scancode** (1 symbols) — Defines keyboard scancodes. Please refer to the Best Keyboard Practices document for details on what this information means and how best to use it. [h
  File: `docs/SDL3/CategoryScancode.md`
- **Mouse** (36 symbols) — Any GUI application has to deal with the mouse, and SDL provides functions to manage mouse input and the displayed cursor. Most interactions with the 
  File: `docs/SDL3/CategoryMouse.md`
- **Joystick** (68 symbols) — SDL joystick support. This is the lower-level joystick handling. If you want the simpler option, where what each button does is well-defined, you shou
  File: `docs/SDL3/CategoryJoystick.md`
- **Gamepad** (80 symbols) — SDL provides a low-level joystick API, which just treats joysticks as an arbitrary pile of buttons, axes, and hat switches. If you're planning to writ
  File: `docs/SDL3/CategoryGamepad.md`
- **Touch** (10 symbols) — SDL offers touch input, on platforms that support it. It can manage multiple touch devices and track multiple fingers on those devices. Touches are mo
  File: `docs/SDL3/CategoryTouch.md`
- **Pen** (17 symbols) — SDL pen event handling. SDL provides an API for pressure-sensitive pen (stylus and/or eraser) handling, e.g., for input and drawing tablets or suitabl
  File: `docs/SDL3/CategoryPen.md`
- **Sensor** (18 symbols) — SDL sensor management. These APIs grant access to gyros and accelerometers on various platforms. In order to use these functions, [SDL_Init](SDL_Init)
  File: `docs/SDL3/CategorySensor.md`
- **HIDAPI** (26 symbols) — Header file for SDL HIDAPI functions. This is an adaptation of the original HIDAPI interface by Alan Ott, and includes source code licensed under the 
  File: `docs/SDL3/CategoryHIDAPI.md`

## Haptic

- **Haptic** (71 symbols) — The SDL haptic subsystem manages haptic (force feedback) devices. The basic usage is as follows: - Initialize the subsystem ([SDL_INIT_HAPTIC](SDL_INI
  File: `docs/SDL3/CategoryHaptic.md`

## Audio

- **Audio** (81 symbols) — Audio functionality for the SDL library. All audio in SDL3 revolves around [SDL_AudioStream](SDL_AudioStream). Whether you want to play or record audi
  File: `docs/SDL3/CategoryAudio.md`

## GPU

- **GPU** (190 symbols) — The GPU API offers a cross-platform way for apps to talk to modern graphics hardware. It offers both 3D graphics and compute support, in the style of 
  File: `docs/SDL3/CategoryGPU.md`

## Threads

- **Thread** (19 symbols) — SDL offers cross-platform thread management functions. These are mostly concerned with starting threads, setting their priority, and dealing with thei
  File: `docs/SDL3/CategoryThread.md`
- **Mutex** (55 symbols) — SDL offers several thread synchronization primitives. This document can't cover the complicated topic of thread safety, but reading up on what each of
  File: `docs/SDL3/CategoryMutex.md`
- **Atomic** (27 symbols) — Atomic operations. IMPORTANT: If you are not an expert in concurrent lockless programming, you should not be using any functions in this file. You sho
  File: `docs/SDL3/CategoryAtomic.md`

## Time

- **Timer** (24 symbols) — SDL provides time management functionality. It is useful for dealing with (usually) small durations of time. This is not to be confused with *calendar
  File: `docs/SDL3/CategoryTimer.md`
- **Time** (12 symbols) — SDL realtime clock and date/time routines. There are two data types that are used in this category: [SDL_Time](SDL_Time), which represents the nanosec
  File: `docs/SDL3/CategoryTime.md`

## File & I/O

- **Filesystem** (17 symbols) — SDL offers an API for examining and manipulating the system's filesystem. This covers most things one would need to do with directories, except for ac
  File: `docs/SDL3/CategoryFilesystem.md`
- **Storage** (19 symbols) — The storage API is a high-level API designed to abstract away the portability issues that come up when using something lower-level (in SDL's case, thi
  File: `docs/SDL3/CategoryStorage.md`
- **IOStream** (52 symbols) — SDL provides an abstract interface for reading and writing data streams. It offers implementations for files, memory, etc, and the app can provide the
  File: `docs/SDL3/CategoryIOStream.md`
- **AsyncIO** (16 symbols) — SDL offers a way to perform I/O asynchronously. This allows an app to read or write files without waiting for data to actually transfer; the functions
  File: `docs/SDL3/CategoryAsyncIO.md`

## Platform & CPU

- **Platform** (37 symbols) — SDL provides a means to identify the app's platform, both at compile time and runtime.
  File: `docs/SDL3/CategoryPlatform.md`
- **CPUInfo** (20 symbols) — CPU feature detection for SDL. These functions are largely concerned with reporting if the system has access to various SIMD instruction sets, but als
  File: `docs/SDL3/CategoryCPUInfo.md`
- **Intrinsics** (15 symbols) — SDL does some preprocessor gymnastics to determine if any CPU-specific compiler intrinsics are available, as this is not necessarily an easy thing to 
  File: `docs/SDL3/CategoryIntrinsics.md`
- **Endian** (16 symbols) — Functions converting endian-specific values to different byte orders. These functions either unconditionally swap byte order ([SDL_Swap16](SDL_Swap16)
  File: `docs/SDL3/CategoryEndian.md`
- **Bits** (2 symbols) — Functions for fiddling with bits and bitmasks.
  File: `docs/SDL3/CategoryBits.md`

## System

- **Power** (2 symbols) — SDL power management routines. There is a single function in this category: [SDL_GetPowerInfo](SDL_GetPowerInfo)(). This function is useful for games 
  File: `docs/SDL3/CategoryPower.md`
- **Messagebox** (9 symbols) — SDL offers a simple message box API, which is useful for simple alerts, such as informing the user when something fatal happens at startup without the
  File: `docs/SDL3/CategoryMessagebox.md`
- **Dialog** (7 symbols) — File dialog support. SDL offers file dialogs, to let users select files with native GUI interfaces. There are "open" dialogs, "save" dialogs, and fold
  File: `docs/SDL3/CategoryDialog.md`
- **Tray** (30 symbols) — SDL offers a way to add items to the "system tray" (more correctly called the "notification area" on Windows). On platforms that offer this concept, a
  File: `docs/SDL3/CategoryTray.md`
- **Process** (11 symbols) — Process control support. These functions provide a cross-platform way to spawn and manage OS-level processes. You can create a new subprocess with [SD
  File: `docs/SDL3/CategoryProcess.md`
- **SharedObject** (4 symbols) — System-dependent library loading routines. Shared objects are code that is programmatically loadable at runtime. Windows calls these "DLLs", Linux cal
  File: `docs/SDL3/CategorySharedObject.md`
- **Locale** (2 symbols) — SDL locale services. This provides a way to get a list of preferred locales (language plus country) for the user. There is exactly one function: [SDL_
  File: `docs/SDL3/CategoryLocale.md`
- **Misc** (1 symbols) — SDL API functions that don't fit elsewhere.
  File: `docs/SDL3/CategoryMisc.md`
- **Stdinc** (242 symbols) — SDL provides its own implementation of some of the most important C runtime functions. Using these functions allows an app to have access to common C 
  File: `docs/SDL3/CategoryStdinc.md`
- **System** (41 symbols) — Platform-specific SDL API functions. These are functions that deal with needs of specific operating systems, that didn't make sense to offer as platfo
  File: `docs/SDL3/CategorySystem.md`

## Other

- **Android** (1 symbols) — 
  File: `docs/SDL3/CategoryAndroid.md`
- **BeginCode** (14 symbols) — `SDL_begin_code.h` sets things up for C dynamic library function definitions, static inlined functions, and structures aligned at 4-byte alignment. If
  File: `docs/SDL3/CategoryBeginCode.md`
- **DlopenNotes** (4 symbols) — This header allows you to annotate your code so external tools know about dynamic shared library dependencies. If you determine that your toolchain do
  File: `docs/SDL3/CategoryDlopenNotes.md`
- **GUID** (3 symbols) — A GUID is a 128-bit value that represents something that is uniquely identifiable by this value: "globally unique." SDL provides functions to convert 
  File: `docs/SDL3/CategoryGUID.md`
- **Openxr** (7 symbols) — Functions for creating OpenXR handles for [SDL_gpu](SDL_gpu) contexts. For the most part, OpenXR operates independent of SDL, but the graphics initial
  File: `docs/SDL3/CategoryOpenxr.md`

## Extension Libraries

- **SDL3_image** (163 symbols) — File: `docs/SDL3_image/`
- **SDL3_mixer** (152 symbols) — File: `docs/SDL3_mixer/`
- **SDL3_ttf** (205 symbols) — File: `docs/SDL3_ttf/`
- **SDL3_net** (50 symbols) — File: `docs/SDL3_net/`

