#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>

#include "EngineBackend.h"

#if defined(__has_include)
#if __has_include(<raylib.h>)
#define RAYQUIRO_HAS_RAYLIB 1
#include <raylib.h>
#include <rlgl.h>
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

inline std::unordered_map<int64_t, Model>& model_registry() {
    static std::unordered_map<int64_t, Model> s_models;
    return s_models;
}
inline int64_t next_model_id() {
    static int64_t s_id = 1;
    return s_id++;
}

inline std::unordered_map<int64_t, Texture2D>& texture_registry() {
    static std::unordered_map<int64_t, Texture2D> s_textures;
    return s_textures;
}
inline int64_t next_texture_id() {
    static int64_t s_id = 1;
    return s_id++;
}

inline int64_t load_model(const char* filename) {
    if (!rte::backendState.windowReady || !filename) return 0;
    Model m = LoadModel(filename);
    if (m.meshCount == 0) return 0;
    int64_t id = next_model_id();
    model_registry()[id] = m;
    return id;
}

inline void unload_model(int64_t id) {
    auto& reg = model_registry();
    auto it = reg.find(id);
    if (it != reg.end()) {
        UnloadModel(it->second);
        reg.erase(it);
    }
}

inline void draw_model(int64_t id, RTVec3 position, float scale, RTColor tint) {
    if (!rte::backendState.windowReady) return;
    auto& reg = model_registry();
    auto it = reg.find(id);
    if (it == reg.end()) return;
    rte::ensure_mode3d();
    DrawModel(it->second, rte::convert_vec3(position), scale, rte::convert_color(tint));
    rte::backendState.frameDrawCalls += 1;
}

inline void draw_model_ex(int64_t id, RTVec3 position, RTVec3 rotationAxis, float rotationAngle, RTVec3 scale, RTColor tint) {
    if (!rte::backendState.windowReady) return;
    auto& reg = model_registry();
    auto it = reg.find(id);
    if (it == reg.end()) return;
    rte::ensure_mode3d();
    DrawModelEx(it->second, rte::convert_vec3(position), rte::convert_vec3(rotationAxis), rotationAngle, rte::convert_vec3(scale), rte::convert_color(tint));
    rte::backendState.frameDrawCalls += 1;
}

inline int64_t load_texture(const char* filename) {
    if (!rte::backendState.windowReady || !filename) return 0;
    Texture2D t = LoadTexture(filename);
    if (t.id == 0) return 0;
    int64_t id = next_texture_id();
    texture_registry()[id] = t;
    return id;
}

inline void unload_texture(int64_t id) {
    auto& reg = texture_registry();
    auto it = reg.find(id);
    if (it != reg.end()) {
        UnloadTexture(it->second);
        reg.erase(it);
    }
}

inline void draw_texture(int64_t id, int posX, int posY, RTColor tint) {
    if (!rte::backendState.windowReady) return;
    auto& reg = texture_registry();
    auto it = reg.find(id);
    if (it == reg.end()) return;
    rte::ensure_mode2d();
    DrawTexture(it->second, posX, posY, rte::convert_color(tint));
    rte::backendState.frameDrawCalls += 1;
}

inline void draw_texture_ex(int64_t id, float posX, float posY, float rotation, float scale, RTColor tint) {
    if (!rte::backendState.windowReady) return;
    auto& reg = texture_registry();
    auto it = reg.find(id);
    if (it == reg.end()) return;
    rte::ensure_mode2d();
    DrawTextureEx(it->second, Vector2{posX, posY}, rotation, scale, rte::convert_color(tint));
    rte::backendState.frameDrawCalls += 1;
}

inline void shutdown() {
    if (!rte::backendState.windowReady) {
        return;
    }
    if (rte::backendState.mode3dReady) {
        EndMode3D();
        rte::backendState.mode3dReady = false;
    }
    for (auto& pair : model_registry()) {
        UnloadModel(pair.second);
    }
    model_registry().clear();
    for (auto& pair : texture_registry()) {
        UnloadTexture(pair.second);
    }
    texture_registry().clear();
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
    rte::backendState.mode3dReady = false;
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
    if (rte::backendState.mode3dReady) {
        EndMode3D();
        BeginMode3D(rte::camera);
    }
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

inline float mouse_delta_x() {
    if (!rte::backendState.windowReady) {
        return 0.0f;
    }
    return GetMouseDelta().x;
}

inline float mouse_delta_y() {
    if (!rte::backendState.windowReady) {
        return 0.0f;
    }
    return GetMouseDelta().y;
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

inline Color shade_face_color(RTVec3 faceCenter, RTVec3 normal, RTColor baseColor) {
    float ambR = rte::backendState.lightState.ambientR / 255.0f;
    float ambG = rte::backendState.lightState.ambientG / 255.0f;
    float ambB = rte::backendState.lightState.ambientB / 255.0f;

    float lightR = 0.0f;
    float lightG = 0.0f;
    float lightB = 0.0f;

    if (rte::backendState.lightState.enabled) {
        float lx = rte::backendState.lightState.directionX;
        float ly = rte::backendState.lightState.directionY;
        float lz = rte::backendState.lightState.directionZ;
        float len = std::sqrt(lx * lx + ly * ly + lz * lz);
        if (len > 0.0001f) {
            lx /= len;
            ly /= len;
            lz /= len;
        }
        float dot = -(normal.x * lx + normal.y * ly + normal.z * lz);
        if (dot > 0.0f) {
            float diff = dot * rte::backendState.lightState.intensity;
            lightR += (rte::backendState.lightState.colorR / 255.0f) * diff;
            lightG += (rte::backendState.lightState.colorG / 255.0f) * diff;
            lightB += (rte::backendState.lightState.colorB / 255.0f) * diff;
        }
    } else {
        float lx = -0.4f;
        float ly = 0.85f;
        float lz = -0.35f;
        float len = std::sqrt(lx * lx + ly * ly + lz * lz);
        lx /= len;
        ly /= len;
        lz /= len;
        float dot = normal.x * lx + normal.y * ly + normal.z * lz;
        float diff = std::max(0.0f, dot) * 0.65f;
        lightR += diff;
        lightG += diff;
        lightB += diff;
    }

    for (const auto& pt : rte::backendState.lightState.pointLights) {
        if (!pt.enabled) {
            continue;
        }
        float dx = pt.posX - faceCenter.x;
        float dy = pt.posY - faceCenter.y;
        float dz = pt.posZ - faceCenter.z;
        float distSq = dx * dx + dy * dy + dz * dz;
        float r = pt.radius;
        if (distSq < r * r && distSq > 0.0001f) {
            float dist = std::sqrt(distSq);
            float ldx = dx / dist;
            float ldy = dy / dist;
            float ldz = dz / dist;
            float pDot = normal.x * ldx + normal.y * ldy + normal.z * ldz;
            if (pDot > 0.0f) {
                float normDist = dist / r;
                float atten = (1.0f - normDist) * (1.0f - normDist) * pt.intensity;
                lightR += (pt.colorR / 255.0f) * pDot * atten;
                lightG += (pt.colorG / 255.0f) * pDot * atten;
                lightB += (pt.colorB / 255.0f) * pDot * atten;
            }
        }
    }

    float finalR = std::clamp((baseColor.r / 255.0f) * (ambR + lightR), 0.0f, 1.0f);
    float finalG = std::clamp((baseColor.g / 255.0f) * (ambG + lightG), 0.0f, 1.0f);
    float finalB = std::clamp((baseColor.b / 255.0f) * (ambB + lightB), 0.0f, 1.0f);

    return Color{
        static_cast<unsigned char>(finalR * 255.0f),
        static_cast<unsigned char>(finalG * 255.0f),
        static_cast<unsigned char>(finalB * 255.0f),
        baseColor.a
    };
}

inline void draw_cube(RTVec3 position, RTVec3 size, RTColor color) {
    if (!rte::backendState.windowReady) {
        return;
    }
    rte::ensure_mode3d();
    float x = position.x;
    float y = position.y;
    float z = position.z;
    float hx = size.x * 0.5f;
    float hy = size.y * 0.5f;
    float hz = size.z * 0.5f;

    rlBegin(RL_TRIANGLES);

    Color cFront = shade_face_color(RTVec3{x, y, z + hz}, RTVec3{0.0f, 0.0f, 1.0f}, color);
    rlColor4ub(cFront.r, cFront.g, cFront.b, cFront.a);
    rlNormal3f(0.0f, 0.0f, 1.0f);
    rlVertex3f(x - hx, y - hy, z + hz);
    rlVertex3f(x + hx, y - hy, z + hz);
    rlVertex3f(x - hx, y + hy, z + hz);
    rlVertex3f(x + hx, y + hy, z + hz);
    rlVertex3f(x - hx, y + hy, z + hz);
    rlVertex3f(x + hx, y - hy, z + hz);

    Color cBack = shade_face_color(RTVec3{x, y, z - hz}, RTVec3{0.0f, 0.0f, -1.0f}, color);
    rlColor4ub(cBack.r, cBack.g, cBack.b, cBack.a);
    rlNormal3f(0.0f, 0.0f, -1.0f);
    rlVertex3f(x - hx, y - hy, z - hz);
    rlVertex3f(x - hx, y + hy, z - hz);
    rlVertex3f(x + hx, y - hy, z - hz);
    rlVertex3f(x + hx, y + hy, z - hz);
    rlVertex3f(x + hx, y - hy, z - hz);
    rlVertex3f(x - hx, y + hy, z - hz);

    Color cTop = shade_face_color(RTVec3{x, y + hy, z}, RTVec3{0.0f, 1.0f, 0.0f}, color);
    rlColor4ub(cTop.r, cTop.g, cTop.b, cTop.a);
    rlNormal3f(0.0f, 1.0f, 0.0f);
    rlVertex3f(x - hx, y + hy, z - hz);
    rlVertex3f(x - hx, y + hy, z + hz);
    rlVertex3f(x + hx, y + hy, z + hz);
    rlVertex3f(x + hx, y + hy, z - hz);
    rlVertex3f(x - hx, y + hy, z - hz);
    rlVertex3f(x + hx, y + hy, z + hz);

    Color cBottom = shade_face_color(RTVec3{x, y - hy, z}, RTVec3{0.0f, -1.0f, 0.0f}, color);
    rlColor4ub(cBottom.r, cBottom.g, cBottom.b, cBottom.a);
    rlNormal3f(0.0f, -1.0f, 0.0f);
    rlVertex3f(x - hx, y - hy, z - hz);
    rlVertex3f(x + hx, y - hy, z + hz);
    rlVertex3f(x - hx, y - hy, z + hz);
    rlVertex3f(x + hx, y - hy, z - hz);
    rlVertex3f(x + hx, y - hy, z + hz);
    rlVertex3f(x - hx, y - hy, z - hz);

    Color cRight = shade_face_color(RTVec3{x + hx, y, z}, RTVec3{1.0f, 0.0f, 0.0f}, color);
    rlColor4ub(cRight.r, cRight.g, cRight.b, cRight.a);
    rlNormal3f(1.0f, 0.0f, 0.0f);
    rlVertex3f(x + hx, y - hy, z - hz);
    rlVertex3f(x + hx, y + hy, z - hz);
    rlVertex3f(x + hx, y + hy, z + hz);
    rlVertex3f(x + hx, y - hy, z + hz);
    rlVertex3f(x + hx, y - hy, z - hz);
    rlVertex3f(x + hx, y + hy, z + hz);

    Color cLeft = shade_face_color(RTVec3{x - hx, y, z}, RTVec3{-1.0f, 0.0f, 0.0f}, color);
    rlColor4ub(cLeft.r, cLeft.g, cLeft.b, cLeft.a);
    rlNormal3f(-1.0f, 0.0f, 0.0f);
    rlVertex3f(x - hx, y - hy, z - hz);
    rlVertex3f(x - hx, y + hy, z + hz);
    rlVertex3f(x - hx, y + hy, z - hz);
    rlVertex3f(x - hx, y - hy, z + hz);
    rlVertex3f(x - hx, y + hy, z + hz);
    rlVertex3f(x - hx, y - hy, z - hz);

    rlEnd();
    rte::backendState.frameDrawCalls += 1;
}

inline void draw_plane(RTVec3 position, RTVec2 size, RTColor color) {
    if (!rte::backendState.windowReady) {
        return;
    }
    rte::ensure_mode3d();
    float x = position.x;
    float y = position.y;
    float z = position.z;
    float hx = size.x * 0.5f;
    float hz = size.y * 0.5f;
    Color c = shade_face_color(position, RTVec3{0.0f, 1.0f, 0.0f}, color);
    rlBegin(RL_TRIANGLES);
    rlColor4ub(c.r, c.g, c.b, c.a);
    rlNormal3f(0.0f, 1.0f, 0.0f);
    rlVertex3f(x - hx, y, z - hz);
    rlVertex3f(x - hx, y, z + hz);
    rlVertex3f(x + hx, y, z + hz);
    rlVertex3f(x + hx, y, z - hz);
    rlVertex3f(x - hx, y, z - hz);
    rlVertex3f(x + hx, y, z + hz);
    rlEnd();
    rte::backendState.frameDrawCalls += 1;
}

inline void draw_sphere(RTVec3 position, float radius, RTColor color) {
    if (!rte::backendState.windowReady) {
        return;
    }
    rte::ensure_mode3d();
    Color c = shade_face_color(position, RTVec3{0.0f, 1.0f, 0.0f}, color);
    DrawSphere(rte::convert_vec3(position), radius, c);
    rte::backendState.frameDrawCalls += 1;
}

inline void draw_cylinder(RTVec3 position, float radiusTop, float radiusBottom, float height, int slices, RTColor color) {
    if (!rte::backendState.windowReady) {
        return;
    }
    rte::ensure_mode3d();
    if (slices < 3) {
        slices = 16;
    }
    float h = height * 0.5f;
    float step = 6.28318530718f / static_cast<float>(slices);
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < slices; ++i) {
        float a1 = static_cast<float>(i) * step;
        float a2 = static_cast<float>(i + 1) * step;
        float cos1 = std::cos(a1);
        float sin1 = std::sin(a1);
        float cos2 = std::cos(a2);
        float sin2 = std::sin(a2);
        float midA = (a1 + a2) * 0.5f;
        RTVec3 nMid{std::cos(midA), 0.0f, std::sin(midA)};
        RTVec3 centerMid{position.x + nMid.x * radiusBottom, position.y, position.z + nMid.z * radiusBottom};
        Color cSide = shade_face_color(centerMid, nMid, color);
        rlColor4ub(cSide.r, cSide.g, cSide.b, cSide.a);
        rlNormal3f(cos1, 0.0f, sin1);
        rlVertex3f(position.x + cos1 * radiusBottom, position.y - h, position.z + sin1 * radiusBottom);
        rlNormal3f(cos2, 0.0f, sin2);
        rlVertex3f(position.x + cos2 * radiusBottom, position.y - h, position.z + sin2 * radiusBottom);
        rlNormal3f(cos1, 0.0f, sin1);
        rlVertex3f(position.x + cos1 * radiusTop, position.y + h, position.z + sin1 * radiusTop);

        rlNormal3f(cos2, 0.0f, sin2);
        rlVertex3f(position.x + cos2 * radiusBottom, position.y - h, position.z + sin2 * radiusBottom);
        rlNormal3f(cos2, 0.0f, sin2);
        rlVertex3f(position.x + cos2 * radiusTop, position.y + h, position.z + sin2 * radiusTop);
        rlNormal3f(cos1, 0.0f, sin1);
        rlVertex3f(position.x + cos1 * radiusTop, position.y + h, position.z + sin1 * radiusTop);

        if (radiusTop > 0.0f) {
            Color cTop = shade_face_color(RTVec3{position.x, position.y + h, position.z}, RTVec3{0.0f, 1.0f, 0.0f}, color);
            rlColor4ub(cTop.r, cTop.g, cTop.b, cTop.a);
            rlNormal3f(0.0f, 1.0f, 0.0f);
            rlVertex3f(position.x, position.y + h, position.z);
            rlVertex3f(position.x + cos1 * radiusTop, position.y + h, position.z + sin1 * radiusTop);
            rlVertex3f(position.x + cos2 * radiusTop, position.y + h, position.z + sin2 * radiusTop);
        }
        if (radiusBottom > 0.0f) {
            Color cBot = shade_face_color(RTVec3{position.x, position.y - h, position.z}, RTVec3{0.0f, -1.0f, 0.0f}, color);
            rlColor4ub(cBot.r, cBot.g, cBot.b, cBot.a);
            rlNormal3f(0.0f, -1.0f, 0.0f);
            rlVertex3f(position.x, position.y - h, position.z);
            rlVertex3f(position.x + cos2 * radiusBottom, position.y - h, position.z + sin2 * radiusBottom);
            rlVertex3f(position.x + cos1 * radiusBottom, position.y - h, position.z + sin1 * radiusBottom);
        }
    }
    rlEnd();
    rte::backendState.frameDrawCalls += 1;
}

inline void draw_wire_cube(RTVec3 position, RTVec3 size, RTColor color) {
    if (!rte::backendState.windowReady) {
        return;
    }
    rte::ensure_mode3d();
    DrawCubeWires(rte::convert_vec3(position), size.x, size.y, size.z, rte::convert_color(color));
    rte::backendState.frameDrawCalls += 1;
}

inline void draw_wire_sphere(RTVec3 position, float radius, RTColor color) {
    if (!rte::backendState.windowReady) {
        return;
    }
    rte::ensure_mode3d();
    DrawSphereWires(rte::convert_vec3(position), radius, 12, 12, rte::convert_color(color));
    rte::backendState.frameDrawCalls += 1;
}

inline void draw_line_3d(RTVec3 start, RTVec3 end, RTColor color) {
    if (!rte::backendState.windowReady) {
        return;
    }
    rte::ensure_mode3d();
    DrawLine3D(rte::convert_vec3(start), rte::convert_vec3(end), rte::convert_color(color));
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

inline void draw_rect(int x, int y, int w, int h, RTColor color) {
    if (!rte::backendState.windowReady) return;
    rte::ensure_mode2d();
    DrawRectangle(x, y, w, h, rte::convert_color(color));
    rte::backendState.frameDrawCalls += 1;
}

inline void draw_rect_lines(int x, int y, int w, int h, RTColor color) {
    if (!rte::backendState.windowReady) return;
    rte::ensure_mode2d();
    DrawRectangleLines(x, y, w, h, rte::convert_color(color));
    rte::backendState.frameDrawCalls += 1;
}

inline void draw_circle(int x, int y, float radius, RTColor color) {
    if (!rte::backendState.windowReady) return;
    rte::ensure_mode2d();
    DrawCircle(x, y, radius, rte::convert_color(color));
    rte::backendState.frameDrawCalls += 1;
}

inline void draw_circle_lines(int x, int y, float radius, RTColor color) {
    if (!rte::backendState.windowReady) return;
    rte::ensure_mode2d();
    DrawCircleLines(x, y, radius, rte::convert_color(color));
    rte::backendState.frameDrawCalls += 1;
}

inline void draw_line(int x1, int y1, int x2, int y2, RTColor color) {
    if (!rte::backendState.windowReady) return;
    rte::ensure_mode2d();
    DrawLine(x1, y1, x2, y2, rte::convert_color(color));
    rte::backendState.frameDrawCalls += 1;
}

inline void draw_pixel(int x, int y, RTColor color) {
    if (!rte::backendState.windowReady) return;
    rte::ensure_mode2d();
    DrawPixel(x, y, rte::convert_color(color));
    rte::backendState.frameDrawCalls += 1;
}

inline int screen_width() {
    return rte::backendState.windowReady ? GetScreenWidth() : 0;
}

inline int screen_height() {
    return rte::backendState.windowReady ? GetScreenHeight() : 0;
}
}
#endif

