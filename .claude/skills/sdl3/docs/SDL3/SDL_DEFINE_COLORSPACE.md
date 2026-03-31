# SDL_DEFINE_COLORSPACE

A macro for defining custom [SDL_Colorspace](SDL_Colorspace) formats.

## Header File

Defined in [<SDL3/SDL_pixels.h>](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_pixels.h)

## Syntax

```c
#define SDL_DEFINE_COLORSPACE(type, range, primaries, transfer, matrix, chroma) \
    (((Uint32)(type) << 28) | ((Uint32)(range) << 24) | ((Uint32)(chroma) << 20) | \
    ((Uint32)(primaries) << 10) | ((Uint32)(transfer) << 5) | ((Uint32)(matrix) << 0))
```

## Macro Parameters

| type | the type of the new format, probably anSDL_ColorTypevalue. |
| --- | --- |
| range | the range of the new format, probably aSDL_ColorRangevalue. |
| primaries | the primaries of the new format, probably anSDL_ColorPrimariesvalue. |
| transfer | the transfer characteristics of the new format, probably anSDL_TransferCharacteristicsvalue. |
| matrix | the matrix coefficients of the new format, probably anSDL_MatrixCoefficientsvalue. |
| chroma | the chroma sample location of the new format, probably anSDL_ChromaLocationvalue. |

## Return Value

Returns a format value in the style of [SDL_Colorspace](SDL_Colorspace).

## Remarks

For example, defining [SDL_COLORSPACE_SRGB](SDL_COLORSPACE_SRGB) looks like
this:

```c
SDL_DEFINE_COLORSPACE(SDL_COLOR_TYPE_RGB,
                      SDL_COLOR_RANGE_FULL,
                      SDL_COLOR_PRIMARIES_BT709,
                      SDL_TRANSFER_CHARACTERISTICS_SRGB,
                      SDL_MATRIX_COEFFICIENTS_IDENTITY,
                      SDL_CHROMA_LOCATION_NONE)
```

## Thread Safety

It is safe to call this macro from any thread.

## Version

This macro is available since SDL 3.2.0.
