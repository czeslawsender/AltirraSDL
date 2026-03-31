# SDL_WindowSupportsGPUPresentMode

Determines whether a presentation mode is supported by the
window.

## Header File

Defined in [<SDL3/SDL_gpu.h>](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_gpu.h)

## Syntax

```c
bool SDL_WindowSupportsGPUPresentMode(
    SDL_GPUDevice *device,
    SDL_Window *window,
    SDL_GPUPresentMode present_mode);
```

## Function Parameters

| SDL_GPUDevice* | device | a GPU context. |
| --- | --- | --- |
| SDL_Window* | window | anSDL_Window. |
| SDL_GPUPresentMode | present_mode | the presentation mode to check. |

## Return Value

(bool) Returns true if supported, false if unsupported.

## Remarks

The window must be claimed before calling this function.

## Version

This function is available since SDL 3.2.0.

## See Also

- [SDL_ClaimWindowForGPUDevice](SDL_ClaimWindowForGPUDevice)
