# SDL_COLORSPACETYPE

A macro to retrieve the type of an [SDL_Colorspace](SDL_Colorspace).

## Header File

Defined in [<SDL3/SDL_pixels.h>](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_pixels.h)

## Syntax

```c
#define SDL_COLORSPACETYPE(cspace)       (SDL_ColorType)(((cspace) >> 28) & 0x0F)
```

## Macro Parameters

| cspace | anSDL_Colorspaceto check. |
| --- | --- |

## Return Value

Returns the [SDL_ColorType](SDL_ColorType) for
`cspace`.

## Thread Safety

It is safe to call this macro from any thread.

## Version

This macro is available since SDL 3.2.0.
