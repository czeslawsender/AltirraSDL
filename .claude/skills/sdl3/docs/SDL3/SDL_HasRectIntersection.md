# SDL_HasRectIntersection

Determine whether two rectangles intersect.

## Header File

Defined in [<SDL3/SDL_rect.h>](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_rect.h)

## Syntax

```c
bool SDL_HasRectIntersection(const SDL_Rect *A, const SDL_Rect *B);
```

## Function Parameters

| constSDL_Rect* | A | anSDL_Rectstructure representing the
first rectangle. |
| --- | --- | --- |
| constSDL_Rect* | B | anSDL_Rectstructure representing the
second rectangle. |

## Return Value

(bool) Returns true if there is an intersection, false otherwise.

## Remarks

If either pointer is NULL the function will return false.

## Thread Safety

It is safe to call this function from any thread.

## Version

This function is available since SDL 3.2.0.

## See Also

- [SDL_GetRectIntersection](SDL_GetRectIntersection)
