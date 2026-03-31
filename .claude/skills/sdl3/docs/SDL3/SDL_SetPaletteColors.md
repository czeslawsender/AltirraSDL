# SDL_SetPaletteColors

Set a range of colors in a palette.

## Header File

Defined in [<SDL3/SDL_pixels.h>](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_pixels.h)

## Syntax

```c
bool SDL_SetPaletteColors(SDL_Palette *palette, const SDL_Color *colors, int firstcolor, int ncolors);
```

## Function Parameters

| SDL_Palette* | palette | theSDL_Palettestructure to
modify. |
| --- | --- | --- |
| constSDL_Color* | colors | an array ofSDL_Colorstructures to
copy into the palette. |
| int | firstcolor | the index of the first palette entry to modify. |
| int | ncolors | the number of entries to modify. |

## Return Value

(bool) Returns true on success or false on failure; call [SDL_GetError](SDL_GetError)() for more information.

## Thread Safety

It is safe to call this function from any thread, as long as the
palette is not modified or destroyed in another thread.

## Version

This function is available since SDL 3.2.0.
