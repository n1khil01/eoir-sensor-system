cmake_minimum_required(VERSION 3.16)
project(eoir_sensor_system CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_executable(eoir_sensor_system
    src/main.cpp
    src/i2c_device.cpp
    src/mlx90640_raw.cpp
    src/frame_processor.cpp
)
target_include_directories(eoir_sensor_system PRIVATE include)
target_link_libraries(eoir_sensor_system PRIVATE Threads::Threads)
