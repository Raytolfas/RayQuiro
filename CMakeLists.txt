cmake_minimum_required(VERSION 3.20)
project(RayQuiro LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(RAYQUIRO_ENABLE_ENGINE "Enable rayquiro.engine support when raylib is available" ON)

add_executable(rqio src/main.cpp)
if(WIN32)
    target_sources(rqio PRIVATE src/resources.rc)
endif()
target_include_directories(rqio PRIVATE include/rayquiro)

if(MSVC)
    target_compile_options(rqio PRIVATE /O2 /EHsc)
else()
    target_compile_options(rqio PRIVATE
        -O2
        -DNDEBUG
        -fvisibility=hidden
        -ffunction-sections
        -fdata-sections
        -fstack-protector-strong
        -fno-ident
    )
    target_link_options(rqio PRIVATE
        -s
        -Wl,--gc-sections
        -Wl,--strip-all
    )
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(rqio PRIVATE -flto)
        target_link_options(rqio PRIVATE -flto)
    endif()
    if(WIN32)
        target_link_options(rqio PRIVATE
            -Wl,--dynamicbase
            -Wl,--nxcompat
            -Wl,--high-entropy-va
        )
    endif()
endif()

if(WIN32)
    target_link_libraries(rqio PRIVATE ws2_32 advapi32 gdi32 user32 winmm)
endif()

if(APPLE)
    target_link_libraries(rqio PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(rqio PRIVATE dl pthread)
endif()

if(RAYQUIRO_ENABLE_ENGINE)
    set(RAYLIB_SRC_ROOT ${CMAKE_SOURCE_DIR}/third_party/raylib/src)
    if(EXISTS ${RAYLIB_SRC_ROOT}/raylib.h)
        target_include_directories(rqio PRIVATE ${RAYLIB_SRC_ROOT} ${RAYLIB_SRC_ROOT}/external/glfw/include)
        target_sources(rqio PRIVATE
            ${RAYLIB_SRC_ROOT}/rcore.c
            ${RAYLIB_SRC_ROOT}/rshapes.c
            ${RAYLIB_SRC_ROOT}/rtextures.c
            ${RAYLIB_SRC_ROOT}/rtext.c
            ${RAYLIB_SRC_ROOT}/rmodels.c
            ${RAYLIB_SRC_ROOT}/raudio.c
            ${RAYLIB_SRC_ROOT}/utils.c
            ${RAYLIB_SRC_ROOT}/rglfw.c
        )
        target_compile_definitions(rqio PRIVATE _GNU_SOURCE PLATFORM_DESKTOP_GLFW GRAPHICS_API_OPENGL_33)

        if(WIN32)
            target_link_libraries(rqio PRIVATE opengl32)
        elseif(APPLE)
            target_link_libraries(rqio PRIVATE "-framework OpenGL")
        else()
            target_link_libraries(rqio PRIVATE GL m rt X11)
        endif()
    endif()
endif()
