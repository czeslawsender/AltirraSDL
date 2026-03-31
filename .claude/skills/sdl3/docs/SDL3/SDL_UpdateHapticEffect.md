# SDL_UpdateHapticEffect

Update the properties of an effect.

## Header File

Defined in [<SDL3/SDL_haptic.h>](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_haptic.h)

## Syntax

```c
bool SDL_UpdateHapticEffect(SDL_Haptic *haptic, SDL_HapticEffectID effect, const SDL_HapticEffect *data);
```

## Function Parameters

| SDL_Haptic* | haptic | theSDL_Hapticdevice that has the
effect. |
| --- | --- | --- |
| SDL_HapticEffectID | effect | the identifier of the effect to update. |
| constSDL_HapticEffect* | data | anSDL_HapticEffectstructure
containing the new effect properties to use. |

## Return Value

(bool) Returns true on success or false on failure; call [SDL_GetError](SDL_GetError)() for more information.

## Remarks

Can be used dynamically, although behavior when dynamically changing
direction may be strange. Specifically the effect may re-upload itself
and start playing from the start. You also cannot change the type either
when running [SDL_UpdateHapticEffect](SDL_UpdateHapticEffect)().

## Version

This function is available since SDL 3.2.0.

## See Also

- [SDL_CreateHapticEffect](SDL_CreateHapticEffect)

- [SDL_RunHapticEffect](SDL_RunHapticEffect)
