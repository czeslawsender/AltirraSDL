# SDL_GetRenderTextureAddressMode

Get the texture addressing mode used in [SDL_RenderGeometry](SDL_RenderGeometry)().

## Header File

Defined in [<SDL3/SDL_render.h>](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_render.h)

## Syntax

```c
bool SDL_GetRenderTextureAddressMode(SDL_Renderer *renderer, SDL_TextureAddressMode *u_mode, SDL_TextureAddressMode *v_mode);
```

## Function Parameters

| SDL_Renderer* | renderer | the rendering context. |
| --- | --- | --- |
| SDL_TextureAddressMode* | u_mode | a pointer filled in with theSDL_TextureAddressModeto use for
horizontal texture coordinates inSDL_RenderGeometry(), may be
NULL. |
| SDL_TextureAddressMode* | v_mode | a pointer filled in with theSDL_TextureAddressModeto use for
vertical texture coordinates inSDL_RenderGeometry(), may be
NULL. |

## Return Value

(bool) Returns true on success or false on failure; call [SDL_GetError](SDL_GetError)() for more information.

## Thread Safety

This function should only be called on the main thread.

## Version

This function is available since SDL 3.4.0.

## See Also

- [SDL_SetRenderTextureAddressMode](SDL_SetRenderTextureAddressMode)
