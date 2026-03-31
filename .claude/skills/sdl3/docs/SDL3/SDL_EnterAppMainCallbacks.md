# SDL_EnterAppMainCallbacks

An entry point for SDL's use in [SDL_MAIN_USE_CALLBACKS](SDL_MAIN_USE_CALLBACKS).

## Header File

Defined in [<SDL3/SDL_main.h>](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_main.h)

## Syntax

```c
int SDL_EnterAppMainCallbacks(int argc, char *argv[], SDL_AppInit_func appinit, SDL_AppIterate_func appiter, SDL_AppEvent_func appevent, SDL_AppQuit_func appquit);
```

## Function Parameters

| int | argc | standard Unix main argc. |
| --- | --- | --- |
| char ** | argv | standard Unix main argv. |
| SDL_AppInit_func | appinit | the application'sSDL_AppInitfunction. |
| SDL_AppIterate_func | appiter | the application'sSDL_AppIteratefunction. |
| SDL_AppEvent_func | appevent | the application'sSDL_AppEventfunction. |
| SDL_AppQuit_func | appquit | the application'sSDL_AppQuitfunction. |

## Return Value

(int) Returns standard Unix main return value.

## Remarks

Generally, you should not call this function directly. This only
exists to hand off work into SDL as soon as possible, where it has a lot
more control and functionality available, and make the inline code in [SDL_main](SDL_main).h as small as possible.

Not all platforms use this, it's actual use is hidden in a magic
header-only library, and you should not call this directly unless you
*really* know what you're doing.

## Thread Safety

It is not safe to call this anywhere except as the only function call
in [SDL_main](SDL_main).

## Version

This function is available since SDL 3.2.0.
