# SDL_GetRectEnclosingPointsFloat

Calculate a minimal rectangle enclosing a set of points with float
precision.

## Header File

Defined in [<SDL3/SDL_rect.h>](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_rect.h)

## Syntax

```c
bool SDL_GetRectEnclosingPointsFloat(const SDL_FPoint *points, int count, const SDL_FRect *clip, SDL_FRect *result);
```

## Function Parameters

| constSDL_FPoint* | points | an array ofSDL_FPointstructures
representing points to be enclosed. |
| --- | --- | --- |
| int | count | the number of structures in thepointsarray. |
| constSDL_FRect* | clip | anSDL_FRectused for clipping or NULL
to enclose all points. |
| SDL_FRect* | result | anSDL_FRectstructure filled in with
the minimal enclosing rectangle. |

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
