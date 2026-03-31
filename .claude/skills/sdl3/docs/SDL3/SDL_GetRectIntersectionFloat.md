# SDL_GetRectIntersectionFloat

Calculate the intersection of two rectangles with float
precision.

## Header File

Defined in [<SDL3/SDL_rect.h>](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_rect.h)

## Syntax

```c
bool SDL_GetRectIntersectionFloat(const SDL_FRect *A, const SDL_FRect *B, SDL_FRect *result);
```

## Function Parameters

| constSDL_FRect* | A | anSDL_FRectstructure representing the
first rectangle. |
| --- | --- | --- |
| constSDL_FRect* | B | anSDL_FRectstructure representing the
second rectangle. |
| SDL_FRect* | result | anSDL_FRectstructure filled in with
the intersection of rectanglesAandB. |

## Return Value

(bool) Returns true if there is an intersection, false otherwise.

## Remarks

If `result` is NULL then this function will return
false.

## Thread Safety

It is safe to call this function from any thread.

## Version

This function is available since SDL 3.2.0.

## See Also

- [SDL_HasRectIntersectionFloat](SDL_HasRectIntersectionFloat)
