cmake_minimum_required(VERSION 3.28)
set(CMAKE_GENERATOR "Ninja")
project(dreame_camera_stream)

# Force linking from sysroot and prevent using host's libstdc++ and libgcc
set(CMAKE_CXX_LINK_FLAGS "--sysroot=${CMAKE_SYSROOT} -L${CMAKE_SYSROOT}/usr/lib")
set(CMAKE_EXE_LINKER_FLAGS "-Wl,-rpath=/usr/lib")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS -fdiagnostics-color=always)

add_executable(camera_stream
    src/main.cpp
    src/Camera.cpp
)

include_directories(include)

target_link_libraries(camera_stream
    pthread
    dl
    rt
    m
    ${CMAKE_SYSROOT}/usr/lib/libstdc++.so
    ${CMAKE_SYSROOT}/usr/lib/libc.so.6
    ${CMAKE_SYSROOT}/usr/lib/libm.so
    ${CMAKE_SYSROOT}/usr/lib/librt.so
    ${CMAKE_SYSROOT}/usr/lib/libgcc_s.so
)