# SDL_BlitSurfaceUnchecked

Perform low-level surface blitting only.

## Header File

Defined in [<SDL3/SDL_surface.h>](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_surface.h)

## Syntax

```c
bool SDL_BlitSurfaceUnchecked(SDL_Surface *src, const SDL_Rect *srcrect, SDL_Surface *dst, const SDL_Rect *dstrect);
```

## Function Parameters

| SDL_Surface* | src | theSDL_Surfacestructure to be
copied from. |
| --- | --- | --- |
| constSDL_Rect* | srcrect | theSDL_Rectstructure representing the
rectangle to be copied, may not be NULL. |
| SDL_Surface* | dst | theSDL_Surfacestructure that is the
blit target. |
| constSDL_Rect* | dstrect | theSDL_Rectstructure representing the
target rectangle in the destination surface, may not be NULL. |

## Return Value

(bool) Returns true on success or false on failure; call [SDL_GetError](SDL_GetError)() for more information.

## Remarks

This is a semi-private blit function and it performs low-level
surface blitting, assuming the input rectangles have already been
clipped.

## Thread Safety

Only one thread should be using the `src` and
`dst` surfaces at any given time.

## Version

This function is available since SDL 3.2.0.

## See Also

- [SDL_BlitSurface](SDL_BlitSurface)
