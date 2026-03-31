# SDL_GetSensorName

Get the implementation dependent name of a sensor.

## Header File

Defined in [<SDL3/SDL_sensor.h>](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_sensor.h)

## Syntax

```c
const char * SDL_GetSensorName(SDL_Sensor *sensor);
```

## Function Parameters

| SDL_Sensor* | sensor | theSDL_Sensorobject. |
| --- | --- | --- |

## Return Value

(const char *) Returns the sensor name or NULL on failure; call [SDL_GetError](SDL_GetError)() for more information.

## Version

This function is available since SDL 3.2.0.
