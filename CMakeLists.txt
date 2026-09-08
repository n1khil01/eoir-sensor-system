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
    src/tcp_server.cpp
)
target_include_directories(eoir_sensor_system PRIVATE include)
target_link_libraries(eoir_sensor_system PRIVATE Threads::Threads)

# Some toolchains (notably 32-bit ARM, and certain libstdc++ builds) emit
# libcalls (__atomic_fetch_add_8, __atomic_load_8, ...) for std::atomic<T>
# ops on 8-byte types instead of inlining lock-free instructions. Link
# libatomic to provide those; it's a no-op on platforms that don't need it.
# Probed with the CXX compiler (not check_library_exists, which drives the C
# compiler and can misfire on a CXX-only project) so the test matches how
# main.cpp is actually compiled and linked.
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_LIBRARIES atomic)
check_cxx_source_compiles("
#include <atomic>
std::atomic<uint64_t> v{0};
int main() { v.fetch_add(1); return static_cast<int>(v.load()); }
" HAVE_LIBATOMIC)
unset(CMAKE_REQUIRED_LIBRARIES)
if(HAVE_LIBATOMIC)
    target_link_libraries(eoir_sensor_system PRIVATE atomic)
endif()
