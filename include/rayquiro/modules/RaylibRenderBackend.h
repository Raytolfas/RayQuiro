#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "EngineBackend.h"

#if defined(__has_include)
#if __has_include(<raylib.h>)
#define RAYQUIRO_HAS_RAYLIB 1
#include <raylib.h>
#else
#define RAYQUIRO_HAS_RAYLIB 0
#endif
#else
#define RAYQUIRO_HAS_RAYLIB 0
#endif

#if RAYQUIRO_HAS_RAYLIB
namespace rte {
inline RTBackendState backendState = {};
inline Camera3D camera = {};

inline Color convert_color(RTColor color) {
    return Color{color.r, color.g, color.b, color.a};
}

inline Vector3 convert_vec3(RTVec3 vector) {
    return Vector3{vector.x, vector.y, vector.z};
}

inline Vector2 convert_vec2(RTVec2 vector) {
    return Vector2{vector.x, vector.y};
}

inline void ensure_drawing() {
    if (!backendState.drawingReady) {
        throw std::runtime_error("Drawing has not started. Call engine.begin() before rendering.");
    }
}

inline void ensure_mode3d() {
    ensure_drawing();
    if (!backendState.mode3dReady) {
        BeginMode3D(camera);
        backendState.mode3dReady = true;
    }
}

inline void ensure_mode2d() {
    ensure_drawing();
    if (backendState.mode3dReady) {
        EndMode3D();
        backendState.mode3dReady = false;
    }
}
}

namespace raylib_backend {
inline void init(int width, int height, const char* title) {
    if (rte::backendState.windowReady) {
        return;
    }

    unsigned int flags = FLAG_WINDOW_RESIZABLE;
    if (rte::backendState.msaaSamples > 1) {
        flags |= FLAG_MSAA_4X_HINT;
    }
    if (rte::backendState.vsyncEnabled) {
        flags |= FLAG_VSYNC_HINT;
    }
    SetConfigFlags(flags);
    InitWindow(width, height, title);
    SetTargetFPS(rte::backendState.targetFps);

    rte::camera.position = Vector3{6.0f, 6.0f, 6.0f};
    rte::camera.target = Vector3{0.0f, 0.0f, 0.0f};
    rte::camera.up = Vector3{0.0f, 1.0f, 0.0f};
    rte::camera.fovy = 45.0f;
    rte::camera.projection = CAMERA_PERSPECTIVE;
    rte::backendState.activeKind = RTBackendKind::Raylib;
    rte::backendState.windowReady = true;
}

inline void shutdown() {
    if (!rte::backendState.windowReady) {
        return;
    }
    if (rte::backendState.mode3dReady) {
        EndMode3D();
        rte::backendState.mode3dReady = false;
    }
    CloseWindow();
    rte::backendState.windowReady = false;
    rte::backendState.drawingReady = false;
    rte::backendState.frameDrawCalls = 0;
}

inline int should_close() {
    if (!rte::backendState.windowReady) {
        return 1;
    }
    return WindowShouldClose() ? 1 : 0;
}

inline void begin() {
    if (!rte::backendState.windowReady) {
        throw std::runtime_error("rt_begin() called before rt_init().");
    }

    BeginDrawing();
    rte::backendState.drawingReady = true;
    rte::backendState.frameDrawCalls = 0;
}

inline void end() {
    if (!rte::backendState.windowReady) {
        return;
    }

    rte::ensure_mode2d();
    EndDrawing();
    rte::backendState.drawingReady = false;
}

inline void clear(RTColor color) {
    if (!rte::backendState.windowReady) {
        return;
    }
    rte::ensure_mode2d();
    ClearBackground(rte::convert_color(color));
}

inline void set_camera(float px, float py, float pz, float tx, float ty, float tz, float ux, float uy, float uz, float fov) {
    rte::camera.position = Vector3{px, py, pz};
    rte::camera.target = Vector3{tx, ty, tz};
    rte::camera.up = Vector3{ux, uy, uz};
    rte::camera.fovy = fov;
}

inline float camera_fov() {
    return rte::camera.fovy;
}

inline void set_camera_fov(float fov) {
    rte::camera.fovy = fov;
}

inline void camera_orbit(float yawDegrees, float pitchDegrees, float radiusDelta) {
    Vector3 offset = {
        rte::camera.position.x - rte::camera.target.x,
        rte::camera.position.y - rte::camera.target.y,
        rte::camera.position.z - rte::camera.target.z
    };

    float radius = std::sqrt(offset.x * offset.x + offset.y * offset.y + offset.z * offset.z);
    if (radius < 0.001f) {
        radius = 0.001f;
    }

    float yaw = std::atan2(offset.z, offset.x);
    float pitch = std::asin(offset.y / radius);
    constexpr float degToRad = 3.14159265358979323846f / 180.0f;

    yaw += yawDegrees * degToRad;
    pitch += pitchDegrees * degToRad;
    pitch = std::clamp(pitch, -1.4f, 1.4f);
    radius = std::max(0.5f, radius + radiusDelta);

    offset.x = std::cos(pitch) * std::cos(yaw) * radius;
    offset.y = std::sin(pitch) * radius;
    offset.z = std::cos(pitch) * std::sin(yaw) * radius;

    rte::camera.position = Vector3{
        rte::camera.target.x + offset.x,
        rte::camera.target.y + offset.y,
        rte::camera.target.z + offset.z
    };
}

inline void set_target_fps(int fps) {
    rte::backendState.targetFps = fps;
    if (rte::backendState.windowReady) {
        SetTargetFPS(fps);
    }
}

inline float frame_time() {
    if (!rte::backendState.windowReady) {
        return 0.0f;
    }
    return GetFrameTime();
}

inline int key_down(int key) {
    if (!rte::backendState.windowReady) {
        return 0;
    }
    return IsKeyDown(key) ? 1 : 0;
}

inline int key_pressed(int key) {
    if (!rte::backendState.windowReady) {
        return 0;
    }
    return IsKeyPressed(key) ? 1 : 0;
}

inline int mouse_down(int button) {
    if (!rte::backendState.windowReady) {
        return 0;
    }
    return IsMouseButtonDown(button) ? 1 : 0;
}

inline float mouse_x() {
    if (!rte::backendState.windowReady) {
        return 0.0f;
    }
    return static_cast<float>(GetMouseX());
}

inline float mouse_y() {
    if (!rte::backendState.windowReady) {
        return 0.0f;
    }
    return static_cast<float>(GetMouseY());
}

inline int gpu_count() {
    return 1;
}

inline void draw_grid(int slices, float spacing) {
    if (!rte::backendState.windowReady) {
        return;
    }
    rte::ensure_mode3d();
    DrawGrid(slices, spacing);
    rte::backendState.frameDrawCalls += 1;
}

inline void draw_cube(RTVec3 position, RTVec3 size, RTColor color) {
    if (!rte::backendState.windowReady) {
        return;
    }
    rte::ensure_mode3d();
    DrawCubeV(rte::convert_vec3(position), rte::convert_vec3(size), rte::convert_color(color));
    rte::backendState.frameDrawCalls += 1;
}

inline void draw_plane(RTVec3 position, RTVec2 size, RTColor color) {
    if (!rte::backendState.windowReady) {
        return;
    }
    rte::ensure_mode3d();
    DrawPlane(rte::convert_vec3(position), rte::convert_vec2(size), rte::convert_color(color));
    rte::backendState.frameDrawCalls += 1;
}

inline void draw_sphere(RTVec3 position, float radius, RTColor color) {
    if (!rte::backendState.windowReady) {
        return;
    }
    rte::ensure_mode3d();
    DrawSphere(rte::convert_vec3(position), radius, rte::convert_color(color));
    rte::backendState.frameDrawCalls += 1;
}

inline void draw_text(const char* text, int x, int y, int size, RTColor color) {
    if (!rte::backendState.windowReady) {
        return;
    }
    rte::ensure_mode2d();
    DrawText(text, x, y, size, rte::convert_color(color));
    rte::backendState.frameDrawCalls += 1;
}

inline void draw_fps(int x, int y) {
    if (!rte::backendState.windowReady) {
        return;
    }
    rte::ensure_mode2d();
    DrawFPS(x, y);
    rte::backendState.frameDrawCalls += 1;
}
}
#endif
