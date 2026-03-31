# SDL_ReleaseWindowFromGPUDevice

Unclaims a window, destroying its swapchain structure.

## Header File

Defined in [<SDL3/SDL_gpu.h>](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_gpu.h)

## Syntax

```c
void SDL_ReleaseWindowFromGPUDevice(
    SDL_GPUDevice *device,
    SDL_Window *window);
```

## Function Parameters

| SDL_GPUDevice* | device | a GPU context. |
| --- | --- | --- |
| SDL_Window* | window | anSDL_Windowthat has been
claimed. |

## Version

This function is available since SDL 3.2.0.

## See Also

- [SDL_ClaimWindowForGPUDevice](SDL_ClaimWindowForGPUDevice)
