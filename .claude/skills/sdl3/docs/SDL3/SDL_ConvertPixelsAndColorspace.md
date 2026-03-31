# SDL_ConvertPixelsAndColorspace

Copy a block of pixels of one format and colorspace to another format
and colorspace.

## Header File

Defined in [<SDL3/SDL_surface.h>](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_surface.h)

## Syntax

```c
bool SDL_ConvertPixelsAndColorspace(int width, int height, SDL_PixelFormat src_format, SDL_Colorspace src_colorspace, SDL_PropertiesID src_properties, const void *src, int src_pitch, SDL_PixelFormat dst_format, SDL_Colorspace dst_colorspace, SDL_PropertiesID dst_properties, void *dst, int dst_pitch);
```

## Function Parameters

| int | width | the width of the block to copy, in pixels. |
| --- | --- | --- |
| int | height | the height of the block to copy, in pixels. |
| SDL_PixelFormat | src_format | anSDL_PixelFormatvalue of thesrcpixels format. |
| SDL_Colorspace | src_colorspace | anSDL_Colorspacevalue describing
the colorspace of thesrcpixels. |
| SDL_PropertiesID | src_properties | anSDL_PropertiesIDwith
additional source color properties, or 0. |
| const void * | src | a pointer to the source pixels. |
| int | src_pitch | the pitch of the source pixels, in bytes. |
| SDL_PixelFormat | dst_format | anSDL_PixelFormatvalue of thedstpixels format. |
| SDL_Colorspace | dst_colorspace | anSDL_Colorspacevalue describing
the colorspace of thedstpixels. |
| SDL_PropertiesID | dst_properties | anSDL_PropertiesIDwith
additional destination color properties, or 0. |
| void * | dst | a pointer to be filled in with new pixel data. |
| int | dst_pitch | the pitch of the destination pixels, in bytes. |

## Return Value

(bool) Returns true on success or false on failure; call [SDL_GetError](SDL_GetError)() for more information.

## Thread Safety

The same destination pixels should not be used from two threads at
once. It is safe to use the same source pixels from multiple
threads.

## Version

This function is available since SDL 3.2.0.

## See Also

- [SDL_ConvertPixels](SDL_ConvertPixels)
