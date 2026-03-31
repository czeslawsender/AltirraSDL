# SDL_HitTest

Callback used for hit-testing.

## Header File

Defined in [<SDL3/SDL_video.h>](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_video.h)

## Syntax

```c
typedef SDL_HitTestResult (SDLCALL *SDL_HitTest)(SDL_Window *win, const SDL_Point *area, void *data);
```

## Function Parameters

| win | theSDL_Windowwhere hit-testing was
set on. |
| --- | --- |
| area | anSDL_Pointwhich should be
hit-tested. |
| data | what was passed ascallback_datatoSDL_SetWindowHitTest(). |

## Return Value

Returns an [SDL_HitTestResult](SDL_HitTestResult)
value.

## See Also

- [SDL_SetWindowHitTest](SDL_SetWindowHitTest)
