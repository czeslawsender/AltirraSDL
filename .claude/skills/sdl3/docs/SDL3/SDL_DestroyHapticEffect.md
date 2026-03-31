# SDL_DestroyHapticEffect

Destroy a haptic effect on the device.

## Header File

Defined in [<SDL3/SDL_haptic.h>](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_haptic.h)

## Syntax

```c
void SDL_DestroyHapticEffect(SDL_Haptic *haptic, SDL_HapticEffectID effect);
```

## Function Parameters

| SDL_Haptic* | haptic | theSDL_Hapticdevice to destroy the
effect on. |
| --- | --- | --- |
| SDL_HapticEffectID | effect | the ID of the haptic effect to destroy. |

## Remarks

This will stop the effect if it's running. Effects are automatically
destroyed when the device is closed.

## Version

This function is available since SDL 3.2.0.

## See Also

- [SDL_CreateHapticEffect](SDL_CreateHapticEffect)
