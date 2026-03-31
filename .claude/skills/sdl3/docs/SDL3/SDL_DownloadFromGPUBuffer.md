# SDL_DownloadFromGPUBuffer

Copies data from a buffer to a transfer buffer on the GPU
timeline.

## Header File

Defined in [<SDL3/SDL_gpu.h>](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_gpu.h)

## Syntax

```c
void SDL_DownloadFromGPUBuffer(
    SDL_GPUCopyPass *copy_pass,
    const SDL_GPUBufferRegion *source,
    const SDL_GPUTransferBufferLocation *destination);
```

## Function Parameters

| SDL_GPUCopyPass* | copy_pass | a copy pass handle. |
| --- | --- | --- |
| constSDL_GPUBufferRegion* | source | the source buffer with offset and size. |
| constSDL_GPUTransferBufferLocation* | destination | the destination transfer buffer with offset. |

## Remarks

This data is not guaranteed to be copied until the command buffer
fence is signaled.

## Version

This function is available since SDL 3.2.0.
