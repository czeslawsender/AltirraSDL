# SDL_SetSurfaceColorspace

Set the colorspace used by a surface.

## Header File

Defined in [<SDL3/SDL_surface.h>](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_surface.h)

## Syntax

```c
bool SDL_SetSurfaceColorspace(SDL_Surface *surface, SDL_Colorspace colorspace);
```

## Function Parameters

| SDL_Surface* | surface | theSDL_Surfacestructure to
update. |
| --- | --- | --- |
| SDL_Colorspace | colorspace | anSDL_Colorspacevalue describing
the surface colorspace. |

## Return Value

(bool) Returns true on success or false on failure; call [SDL_GetError](SDL_GetError)() for more information.

## Remarks

Setting the colorspace doesn't change the pixels, only how they are
interpreted in color operations.

## Thread Safety

This function can be called on different threads with different
surfaces.

## Version

This function is available since SDL 3.2.0.

## See Also

- [SDL_GetSurfaceColorspace](SDL_GetSurfaceColorspace)
