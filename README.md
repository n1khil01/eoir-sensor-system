# EOIR Sensor System

Real-time IR sensor processing pipeline built on embedded Linux (Raspberry Pi 2 Model B) with a companion FreeRTOS component. A capture thread reads frames from an MLX90640 thermal camera into a custom thread-safe ring buffer, a processing thread runs detection, and results stream to a client over TCP.

## Build

```
cmake -S . -B build
cmake --build build
```

## Run

```
./build/eoir_sensor_system
```
