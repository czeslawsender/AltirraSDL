# SDL_SetSurfacePalette

Set the palette used by a surface.

## Header File

Defined in [<SDL3/SDL_surface.h>](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_surface.h)

## Syntax

```c
bool SDL_SetSurfacePalette(SDL_Surface *surface, SDL_Palette *palette);
```

## Function Parameters

| SDL_Surface* | surface | theSDL_Surfacestructure to
update. |
| --- | --- | --- |
| SDL_Palette* | palette | theSDL_Palettestructure to
use. |

## Return Value

(bool) Returns true on success or false on failure; call [SDL_GetError](SDL_GetError)() for more information.

## Remarks

Setting the palette keeps an internal reference to the palette, which
can be safely destroyed afterwards.

A single palette can be shared with many surfaces.

## Thread Safety

This function can be called on different threads with different
surfaces.

## Version

This function is available since SDL 3.2.0.

## See Also

- [SDL_CreatePalette](SDL_CreatePalette)

- [SDL_GetSurfacePalette](SDL_GetSurfacePalette)
