# SDL_GetSensorType

Get the type of a sensor.

## Header File

Defined in [<SDL3/SDL_sensor.h>](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_sensor.h)

## Syntax

```c
SDL_SensorType SDL_GetSensorType(SDL_Sensor *sensor);
```

## Function Parameters

| SDL_Sensor* | sensor | theSDL_Sensorobject to inspect. |
| --- | --- | --- |

## Return Value

([SDL_SensorType](SDL_SensorType)) Returns the [SDL_SensorType](SDL_SensorType) type, or [SDL_SENSOR_INVALID](SDL_SENSOR_INVALID) if
`sensor` is NULL.

## Version

This function is available since SDL 3.2.0.
