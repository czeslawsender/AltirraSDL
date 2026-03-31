###### (This
function is part of SDL_ttf, a separate library from SDL.)

# TTF_GetGlyphScript

Get the script used by a 32-bit codepoint.

## Header File

Defined in [<SDL3_ttf/SDL_ttf.h>](https://github.com/libsdl-org/SDL_ttf/blob/main/include/SDL3_ttf/SDL_ttf.h)

## Syntax

```c
Uint32 TTF_GetGlyphScript(Uint32 ch);
```

## Function Parameters

| Uint32 | ch | the character code to check. |
| --- | --- | --- |

## Return Value

(Uint32) Returns an [ISO 15924
code](https://unicode.org/iso15924/iso15924-codes) on success, or 0 on failure; call SDL_GetError() for more
information.

## Thread Safety

This function is thread-safe.

## Version

This function is available since SDL_ttf 3.0.0.

## See Also

- [TTF_TagToString](TTF_TagToString)
