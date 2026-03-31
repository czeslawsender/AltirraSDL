# SDL_GetRectEnclosingPoints

Calculate a minimal rectangle enclosing a set of points.

## Header File

Defined in [<SDL3/SDL_rect.h>](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_rect.h)

## Syntax

```c
bool SDL_GetRectEnclosingPoints(const SDL_Point *points, int count, const SDL_Rect *clip, SDL_Rect *result);
```

## Function Parameters

| constSDL_Point* | points | an array ofSDL_Pointstructures
representing points to be enclosed. |
| --- | --- | --- |
| int | count | the number of structures in thepointsarray. |
| constSDL_Rect* | clip | anSDL_Rectused for clipping or NULL to
enclose all points. |
| SDL_Rect* | result | anSDL_Rectstructure filled in with the
minimal enclosing rectangle. |

## Return Value

(bool) Returns true if any points were enclosed or false if all the
points were outside of the clipping rectangle.

## Remarks

If `clip` is not NULL then only points inside of the
clipping rectangle are considered.

## Thread Safety

It is safe to call this function from any thread.

## Version

This function is available since SDL 3.2.0.
