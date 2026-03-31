# SDL_BlitSurface9Grid

Perform a scaled blit using the 9-grid algorithm to a destination
surface, which may be of a different format.

## Header File

Defined in [<SDL3/SDL_surface.h>](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_surface.h)

## Syntax

```c
bool SDL_BlitSurface9Grid(SDL_Surface *src, const SDL_Rect *srcrect, int left_width, int right_width, int top_height, int bottom_height, float scale, SDL_ScaleMode scaleMode, SDL_Surface *dst, const SDL_Rect *dstrect);
```

## Function Parameters

| SDL_Surface* | src | theSDL_Surfacestructure to be
copied from. |
| --- | --- | --- |
| constSDL_Rect* | srcrect | theSDL_Rectstructure representing the
rectangle to be used for the 9-grid, or NULL to use the entire
surface. |
| int | left_width | the width, in pixels, of the left corners insrcrect. |
| int | right_width | the width, in pixels, of the right corners insrcrect. |
| int | top_height | the height, in pixels, of the top corners insrcrect. |
| int | bottom_height | the height, in pixels, of the bottom corners insrcrect. |
| float | scale | the scale used to transform the corner ofsrcrectinto
the corner ofdstrect, or 0.0f for an unscaled blit. |
| SDL_ScaleMode | scaleMode | scale algorithm to be used. |
| SDL_Surface* | dst | theSDL_Surfacestructure that is the
blit target. |
| constSDL_Rect* | dstrect | theSDL_Rectstructure representing the
target rectangle in the destination surface, or NULL to fill the entire
surface. |

## Return Value

(bool) Returns true on success or false on failure; call [SDL_GetError](SDL_GetError)() for more information.

## Remarks

The pixels in the source surface are split into a 3x3 grid, using the
different corner sizes for each corner, and the sides and center making
up the remaining pixels. The corners are then scaled using
`scale` and fit into the corners of the destination
rectangle. The sides and center are then stretched into place to cover
the remaining destination rectangle.

## Thread Safety

Only one thread should be using the `src` and
`dst` surfaces at any given time.

## Version

This function is available since SDL 3.2.0.

## See Also

- [SDL_BlitSurface](SDL_BlitSurface)
