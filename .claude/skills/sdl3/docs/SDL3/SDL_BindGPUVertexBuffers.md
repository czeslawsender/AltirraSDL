# SDL_BindGPUVertexBuffers

Binds vertex buffers on a command buffer for use with subsequent draw
calls.

## Header File

Defined in [<SDL3/SDL_gpu.h>](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_gpu.h)

## Syntax

```c
void SDL_BindGPUVertexBuffers(
    SDL_GPURenderPass *render_pass,
    Uint32 first_slot,
    const SDL_GPUBufferBinding *bindings,
    Uint32 num_bindings);
```

## Function Parameters

| SDL_GPURenderPass* | render_pass | a render pass handle. |
| --- | --- | --- |
| Uint32 | first_slot | the vertex buffer slot to begin binding from. |
| constSDL_GPUBufferBinding* | bindings | an array ofSDL_GPUBufferBindingstructs
containing vertex buffers and offset values. |
| Uint32 | num_bindings | the number of bindings in the bindings array. |

## Version

This function is available since SDL 3.2.0.
