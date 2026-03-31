# SDL_ResumeHaptic

Resume a haptic device.

## Header File

Defined in [<SDL3/SDL_haptic.h>](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_haptic.h)

## Syntax

```c
bool SDL_ResumeHaptic(SDL_Haptic *haptic);
```

## Function Parameters

| SDL_Haptic* | haptic | theSDL_Hapticdevice to unpause. |
| --- | --- | --- |

## Return Value

(bool) Returns true on success or false on failure; call [SDL_GetError](SDL_GetError)() for more information.

## Remarks

Call to unpause after [SDL_PauseHaptic](SDL_PauseHaptic)().

## Version

This function is available since SDL 3.2.0.

## See Also

- [SDL_PauseHaptic](SDL_PauseHaptic)
