# SDL_SavePNG_IO

Save a surface to a seekable SDL data stream in PNG format.

## Header File

Defined in [<SDL3/SDL_surface.h>](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_surface.h)

## Syntax

```c
bool SDL_SavePNG_IO(SDL_Surface *surface, SDL_IOStream *dst, bool closeio);
```

## Function Parameters

| SDL_Surface* | surface | theSDL_Surfacestructure containing
the image to be saved. |
| --- | --- | --- |
| SDL_IOStream* | dst | a data stream to save to. |
| bool | closeio | if true, callsSDL_CloseIO() ondstbefore returning, even in the case of an error. |

## Return Value

(bool) Returns true on success or false on failure; call [SDL_GetError](SDL_GetError)() for more information.

## Thread Safety

This function can be called on different threads with different
surfaces.

## Version

This function is available since SDL 3.4.0.

## See Also

- [SDL_LoadPNG_IO](SDL_LoadPNG_IO)

- [SDL_SavePNG](SDL_SavePNG)
