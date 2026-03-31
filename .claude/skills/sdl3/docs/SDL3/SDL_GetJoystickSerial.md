# SDL_GetJoystickSerial

Get the serial number of an opened joystick, if available.

## Header File

Defined in [<SDL3/SDL_joystick.h>](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_joystick.h)

## Syntax

```c
const char * SDL_GetJoystickSerial(SDL_Joystick *joystick);
```

## Function Parameters

| SDL_Joystick* | joystick | theSDL_Joystickobtained fromSDL_OpenJoystick(). |
| --- | --- | --- |

## Return Value

(const char *) Returns the serial number of the selected joystick, or
NULL if unavailable.

## Remarks

Returns the serial number of the joystick, or NULL if it is not
available.

## Thread Safety

It is safe to call this function from any thread.

## Version

This function is available since SDL 3.2.0.
