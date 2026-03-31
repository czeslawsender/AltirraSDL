# SDL_BindGPUComputeSamplers

Binds texture-sampler pairs for use on the compute shader.

## Header File

Defined in [<SDL3/SDL_gpu.h>](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_gpu.h)

## Syntax

```c
void SDL_BindGPUComputeSamplers(
    SDL_GPUComputePass *compute_pass,
    Uint32 first_slot,
    const SDL_GPUTextureSamplerBinding *texture_sampler_bindings,
    Uint32 num_bindings);
```

## Function Parameters

| SDL_GPUComputePass* | compute_pass | a compute pass handle. |
| --- | --- | --- |
| Uint32 | first_slot | the compute sampler slot to begin binding from. |
| constSDL_GPUTextureSamplerBinding* | texture_sampler_bindings | an array of texture-sampler binding structs. |
| Uint32 | num_bindings | the number of texture-sampler bindings to bind from the array. |

## Remarks

The textures must have been created with [SDL_GPU_TEXTUREUSAGE_SAMPLER](SDL_GPU_TEXTUREUSAGE_SAMPLER).

Be sure your shader is set up according to the requirements
documented in [SDL_CreateGPUComputePipeline](SDL_CreateGPUComputePipeline)().

## Version

This function is available since SDL 3.2.0.

## See Also

- [SDL_CreateGPUComputePipeline](SDL_CreateGPUComputePipeline)
