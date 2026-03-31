# SDL_GetRectUnion

Calculate the union of two rectangles.

## Header File

Defined in [<SDL3/SDL_rect.h>](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_rect.h)

## Syntax

```c
bool SDL_GetRectUnion(const SDL_Rect *A, const SDL_Rect *B, SDL_Rect *result);
```

## Function Parameters

| constSDL_Rect* | A | anSDL_Rectstructure representing the
first rectangle. |
| --- | --- | --- |
| constSDL_Rect* | B | anSDL_Rectstructure representing the
second rectangle. |
| SDL_Rect* | result | anSDL_Rectstructure filled in with the
union of rectanglesAandB. |

## Return Value

(bool) Returns true on success or false on failure; call [SDL_GetError](SDL_GetError)() for more information.

## Thread Safety

It is safe to call this function from any thread.

## Version

This function is available since SDL 3.2.0.
