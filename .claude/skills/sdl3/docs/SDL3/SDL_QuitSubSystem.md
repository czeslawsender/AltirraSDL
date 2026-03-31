# SDL_QuitSubSystem

Shut down specific SDL subsystems.

## Header File

Defined in [<SDL3/SDL_init.h>](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_init.h)

## Syntax

```c
void SDL_QuitSubSystem(SDL_InitFlags flags);
```

## Function Parameters

| SDL_InitFlags | flags | any of the flags used bySDL_Init(); seeSDL_Initfor details. |
| --- | --- | --- |

## Remarks

You still need to call [SDL_Quit](SDL_Quit)() even if
you close all open subsystems with [SDL_QuitSubSystem](SDL_QuitSubSystem)().

## Thread Safety

This function is not thread safe.

## Version

This function is available since SDL 3.2.0.

## See Also

- [SDL_InitSubSystem](SDL_InitSubSystem)

- [SDL_Quit](SDL_Quit)
