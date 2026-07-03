#pragma once

#include <stdexcept>
#include <string>

#include "EngineBackend.h"

struct RTColor {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
};

struct RTVec2 {
    float x;
    float y;
};

struct RTVec3 {
    float x;
    float y;
    float z;
};

#include "RaylibRenderBackend.h"
#include "VulkanRenderBackend.h"

#if !RAYQUIRO_HAS_RAYLIB
namespace rte {
inline RTBackendState backendState = {};
}
#endif

inline void rt_init(int width, int height, const char* title) {
    switch (rte::backendState.requestedKind) {
    case RTBackendKind::Vulkan:
        rte::backendState.activeKind = RTBackendKind::Vulkan;
        vulkan_backend::init(width, height, title);
        return;
    case RTBackendKind::Raylib:
    default:
#if RAYQUIRO_HAS_RAYLIB
        raylib_backend::init(width, height, title);
        return;
#else
        (void)width;
        (void)height;
        (void)title;
        throw std::runtime_error("Raylib was not found. Install raylib and build again to use rayquiro.engine.");
#endif
    }
}

inline void rt_attach_host_window(void* parent_window, int x, int y, int width, int height, const char* title) {
    switch (rte::backendState.requestedKind) {
    case RTBackendKind::Vulkan:
        vulkan_backend::attach_host_window(parent_window, x, y, width, height, title);
        return;
    case RTBackendKind::Raylib:
    default:
        (void)parent_window;
        (void)x;
        (void)y;
        (void)width;
        (void)height;
        (void)title;
        return;
    }
}

inline void rt_resize_host_window(int x, int y, int width, int height) {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        vulkan_backend::resize_host_window(x, y, width, height);
        return;
    case RTBackendKind::Raylib:
    default:
        (void)x;
        (void)y;
        (void)width;
        (void)height;
        return;
    }
}

inline void rt_shutdown() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        vulkan_backend::shutdown();
        break;
    case RTBackendKind::Raylib:
    default:
#if RAYQUIRO_HAS_RAYLIB
        raylib_backend::shutdown();
#endif
        break;
    }
}

inline int rt_should_close() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::should_close();
    case RTBackendKind::Raylib:
    default:
#if RAYQUIRO_HAS_RAYLIB
        return raylib_backend::should_close();
#else
        return 1;
#endif
    }
}

inline void rt_begin() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        vulkan_backend::begin();
        return;
    case RTBackendKind::Raylib:
    default:
#if RAYQUIRO_HAS_RAYLIB
        raylib_backend::begin();
#endif
        return;
    }
}

inline void rt_end() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        vulkan_backend::end();
        return;
    case RTBackendKind::Raylib:
    default:
#if RAYQUIRO_HAS_RAYLIB
        raylib_backend::end();
#endif
        return;
    }
}

inline void rt_clear(RTColor color) {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        vulkan_backend::clear(color);
        return;
    case RTBackendKind::Raylib:
    default:
#if RAYQUIRO_HAS_RAYLIB
        raylib_backend::clear(color);
#else
        (void)color;
#endif
        return;
    }
}

inline void rt_set_camera(float px, float py, float pz, float tx, float ty, float tz, float ux, float uy, float uz, float fov) {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        vulkan_backend::set_camera(px, py, pz, tx, ty, tz, ux, uy, uz, fov);
        return;
    case RTBackendKind::Raylib:
    default:
#if RAYQUIRO_HAS_RAYLIB
        raylib_backend::set_camera(px, py, pz, tx, ty, tz, ux, uy, uz, fov);
#else
        (void)px; (void)py; (void)pz; (void)tx; (void)ty; (void)tz; (void)ux; (void)uy; (void)uz; (void)fov;
#endif
        return;
    }
}

inline void rt_camera_orbit(float yawDegrees, float pitchDegrees, float radiusDelta) {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        vulkan_backend::camera_orbit(yawDegrees, pitchDegrees, radiusDelta);
        return;
    case RTBackendKind::Raylib:
    default:
#if RAYQUIRO_HAS_RAYLIB
        raylib_backend::camera_orbit(yawDegrees, pitchDegrees, radiusDelta);
#else
        (void)yawDegrees; (void)pitchDegrees; (void)radiusDelta;
#endif
        return;
    }
}

inline void rt_set_target_fps(int fps) {
    rte::backendState.targetFps = fps;
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        vulkan_backend::set_target_fps(fps);
        return;
    case RTBackendKind::Raylib:
    default:
#if RAYQUIRO_HAS_RAYLIB
        raylib_backend::set_target_fps(fps);
#else
        (void)fps;
#endif
        return;
    }
}

inline int rt_get_target_fps() {
    return rte::backendState.targetFps;
}

inline float rt_get_frame_time() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::frame_time();
    case RTBackendKind::Raylib:
    default:
#if RAYQUIRO_HAS_RAYLIB
        return raylib_backend::frame_time();
#else
        return 0.0f;
#endif
    }
}

inline float rt_get_camera_fov() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::camera_fov();
    case RTBackendKind::Raylib:
    default:
#if RAYQUIRO_HAS_RAYLIB
        return raylib_backend::camera_fov();
#else
        return 45.0f;
#endif
    }
}

inline void rt_set_camera_fov(float fov) {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        vulkan_backend::set_camera_fov(fov);
        return;
    case RTBackendKind::Raylib:
    default:
#if RAYQUIRO_HAS_RAYLIB
        raylib_backend::set_camera_fov(fov);
#else
        (void)fov;
#endif
        return;
    }
}

inline int rt_key_down(int key) {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::key_down(key);
    case RTBackendKind::Raylib:
    default:
#if RAYQUIRO_HAS_RAYLIB
        return raylib_backend::key_down(key);
#else
        (void)key;
        return 0;
#endif
    }
}

inline int rt_key_pressed(int key) {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::key_pressed(key);
    case RTBackendKind::Raylib:
    default:
#if RAYQUIRO_HAS_RAYLIB
        return raylib_backend::key_pressed(key);
#else
        (void)key;
        return 0;
#endif
    }
}

inline int rt_mouse_down(int button) {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::mouse_down(button);
    case RTBackendKind::Raylib:
    default:
#if RAYQUIRO_HAS_RAYLIB
        return raylib_backend::mouse_down(button);
#else
        (void)button;
        return 0;
#endif
    }
}

inline float rt_mouse_x() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::mouse_x();
    case RTBackendKind::Raylib:
    default:
#if RAYQUIRO_HAS_RAYLIB
        return raylib_backend::mouse_x();
#else
        return 0.0f;
#endif
    }
}

inline float rt_mouse_y() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::mouse_y();
    case RTBackendKind::Raylib:
    default:
#if RAYQUIRO_HAS_RAYLIB
        return raylib_backend::mouse_y();
#else
        return 0.0f;
#endif
    }
}

inline void rt_draw_grid(int slices, float spacing) {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        vulkan_backend::draw_grid(slices, spacing);
        return;
    case RTBackendKind::Raylib:
    default:
#if RAYQUIRO_HAS_RAYLIB
        raylib_backend::draw_grid(slices, spacing);
#else
        (void)slices; (void)spacing;
#endif
        return;
    }
}

inline void rt_draw_cube(RTVec3 position, RTVec3 size, RTColor color) {
    rte::backendState.frameDrawCalls += 1;
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        vulkan_backend::draw_cube(position, size, color);
        return;
    case RTBackendKind::Raylib:
    default:
#if RAYQUIRO_HAS_RAYLIB
        raylib_backend::draw_cube(position, size, color);
#else
        (void)position; (void)size; (void)color;
#endif
        return;
    }
}

inline void rt_draw_plane(RTVec3 position, RTVec2 size, RTColor color) {
    rte::backendState.frameDrawCalls += 1;
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        vulkan_backend::draw_plane(position, size, color);
        return;
    case RTBackendKind::Raylib:
    default:
#if RAYQUIRO_HAS_RAYLIB
        raylib_backend::draw_plane(position, size, color);
#else
        (void)position; (void)size; (void)color;
#endif
        return;
    }
}

inline void rt_draw_sphere(RTVec3 position, float radius, RTColor color) {
    rte::backendState.frameDrawCalls += 1;
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        vulkan_backend::draw_sphere(position, radius, color);
        return;
    case RTBackendKind::Raylib:
    default:
#if RAYQUIRO_HAS_RAYLIB
        raylib_backend::draw_sphere(position, radius, color);
#else
        (void)position; (void)radius; (void)color;
#endif
        return;
    }
}

inline void rt_draw_text(const char* text, int x, int y, int size, RTColor color) {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        vulkan_backend::draw_text(text, x, y, size, color);
        return;
    case RTBackendKind::Raylib:
    default:
#if RAYQUIRO_HAS_RAYLIB
        raylib_backend::draw_text(text, x, y, size, color);
#else
        (void)text; (void)x; (void)y; (void)size; (void)color;
#endif
        return;
    }
}

inline void rt_draw_fps(int x, int y) {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        vulkan_backend::draw_fps(x, y);
        return;
    case RTBackendKind::Raylib:
    default:
#if RAYQUIRO_HAS_RAYLIB
        raylib_backend::draw_fps(x, y);
#else
        (void)x; (void)y;
#endif
        return;
    }
}

inline void rt_set_material_state(RTColor albedo, RTColor emissive, float roughness, float metallic, int textured) {
    rte::backendState.materialState.albedoR = albedo.r;
    rte::backendState.materialState.albedoG = albedo.g;
    rte::backendState.materialState.albedoB = albedo.b;
    rte::backendState.materialState.albedoA = albedo.a;
    rte::backendState.materialState.emissiveR = emissive.r;
    rte::backendState.materialState.emissiveG = emissive.g;
    rte::backendState.materialState.emissiveB = emissive.b;
    rte::backendState.materialState.emissiveA = emissive.a;
    rte::backendState.materialState.roughness = roughness;
    rte::backendState.materialState.metallic = metallic;
    rte::backendState.materialState.textured = textured != 0;
}

inline void rt_set_material_texture_source(const char* source) {
    rte::backendState.materialState.textureSource = (source == nullptr) ? std::string() : std::string(source);
}

inline void rt_set_light_state(RTColor ambient, RTVec3 direction, RTColor color, float intensity, int enabled) {
    rte::backendState.lightState.ambientR = ambient.r;
    rte::backendState.lightState.ambientG = ambient.g;
    rte::backendState.lightState.ambientB = ambient.b;
    rte::backendState.lightState.ambientA = ambient.a;
    rte::backendState.lightState.directionX = direction.x;
    rte::backendState.lightState.directionY = direction.y;
    rte::backendState.lightState.directionZ = direction.z;
    rte::backendState.lightState.colorR = color.r;
    rte::backendState.lightState.colorG = color.g;
    rte::backendState.lightState.colorB = color.b;
    rte::backendState.lightState.colorA = color.a;
    rte::backendState.lightState.intensity = intensity;
    rte::backendState.lightState.enabled = enabled != 0;
}

inline void rt_set_postfx_state(
    float exposure,
    float vignette,
    float filmGrain,
    float saturation,
    float contrast,
    float bloom,
    RTColor fogColor,
    float fogNear,
    float fogFar,
    float fogDensity,
    float volumetric) {
    rte::backendState.postfxState.exposure = exposure;
    rte::backendState.postfxState.vignette = vignette;
    rte::backendState.postfxState.filmGrain = filmGrain;
    rte::backendState.postfxState.saturation = saturation;
    rte::backendState.postfxState.contrast = contrast;
    rte::backendState.postfxState.bloom = bloom;
    rte::backendState.postfxState.fogR = fogColor.r;
    rte::backendState.postfxState.fogG = fogColor.g;
    rte::backendState.postfxState.fogB = fogColor.b;
    rte::backendState.postfxState.fogA = fogColor.a;
    rte::backendState.postfxState.fogNear = fogNear;
    rte::backendState.postfxState.fogFar = fogFar;
    rte::backendState.postfxState.fogDensity = fogDensity;
    rte::backendState.postfxState.volumetric = volumetric;
}

inline void rt_upload_texture_rgba(int width, int height, const unsigned char* pixels) {
    if (width <= 0 || height <= 0 || pixels == nullptr) {
        return;
    }
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        vulkan_backend::upload_scene_texture_rgba(width, height, pixels);
        return;
    case RTBackendKind::Raylib:
    default:
        return;
    }
}

inline void rt_set_backend(const char* name) {
    if (rte::backendState.windowReady) {
        throw std::runtime_error("Set engine.backend() before engine.window()/engine.init().");
    }
    rte::backendState.requestedKind = rt_parse_backend_kind((name == nullptr || name[0] == '\0') ? "raylib" : std::string(name));
    rte::backendState.activeKind = rte::backendState.requestedKind;
}

inline const char* rt_backend_name() {
    static std::string name;
    name = rt_backend_name_from_kind(rte::backendState.activeKind);
    return name.c_str();
}

inline const char* rt_backend_requested_name() {
    static std::string name;
    name = rt_backend_name_from_kind(rte::backendState.requestedKind);
    return name.c_str();
}

inline void rt_set_vsync(int enabled) {
    if (rte::backendState.windowReady) {
        throw std::runtime_error("Set engine.vsync() before engine.window()/engine.init().");
    }
    rte::backendState.vsyncEnabled = enabled != 0;
}

inline int rt_get_vsync() {
    return rte::backendState.vsyncEnabled ? 1 : 0;
}

inline void rt_set_msaa(int samples) {
    if (rte::backendState.windowReady) {
        throw std::runtime_error("Set engine.msaa() before engine.window()/engine.init().");
    }
    rte::backendState.msaaSamples = samples < 1 ? 1 : samples;
}

inline int rt_get_msaa() {
    return rte::backendState.msaaSamples;
}

inline int rt_get_draw_calls() {
    return rte::backendState.frameDrawCalls;
}

inline void rt_set_render_items(int count) {
    rte::backendState.frameRenderItems = count;
}

inline int rt_get_render_items() {
    return rte::backendState.frameRenderItems;
}

inline int rt_backend_gpu_count() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::gpu_count();
    case RTBackendKind::Raylib:
    default:
#if RAYQUIRO_HAS_RAYLIB
        return raylib_backend::gpu_count();
#else
        return 0;
#endif
    }
}

inline int rt_backend_surface_ready() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::surface_ready();
    case RTBackendKind::Raylib:
    default:
        return 0;
    }
}

inline int rt_backend_device_ready() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::device_ready();
    case RTBackendKind::Raylib:
    default:
        return 0;
    }
}

inline int rt_backend_presentation_ready() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::presentation_ready();
    case RTBackendKind::Raylib:
    default:
        return 0;
    }
}

inline int rt_backend_queue_family_index() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::queue_family_index();
    case RTBackendKind::Raylib:
    default:
        return -1;
    }
}

inline int rt_backend_swapchain_ready() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::swapchain_ready();
    case RTBackendKind::Raylib:
    default:
        return 0;
    }
}

inline int rt_backend_swapchain_image_count() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::swapchain_image_count();
    case RTBackendKind::Raylib:
    default:
        return 0;
    }
}

inline int rt_backend_swapchain_width() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::swapchain_width();
    case RTBackendKind::Raylib:
    default:
        return 0;
    }
}

inline int rt_backend_swapchain_height() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::swapchain_height();
    case RTBackendKind::Raylib:
    default:
        return 0;
    }
}

inline int rt_backend_render_pass_ready() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::render_pass_ready();
    case RTBackendKind::Raylib:
    default:
        return 0;
    }
}

inline int rt_backend_framebuffer_count() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::framebuffer_count();
    case RTBackendKind::Raylib:
    default:
        return 0;
    }
}

inline int rt_backend_depth_ready() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::depth_ready();
    case RTBackendKind::Raylib:
    default:
        return 0;
    }
}

inline int rt_backend_geometry_buffers_ready() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::geometry_buffers_ready();
    case RTBackendKind::Raylib:
    default:
        return 0;
    }
}

inline int rt_backend_vertex_buffer_bytes() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::vertex_buffer_bytes();
    case RTBackendKind::Raylib:
    default:
        return 0;
    }
}

inline int rt_backend_index_buffer_bytes() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::index_buffer_bytes();
    case RTBackendKind::Raylib:
    default:
        return 0;
    }
}

inline int rt_backend_shader_assets_ready() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::shader_assets_ready();
    case RTBackendKind::Raylib:
    default:
        return 0;
    }
}

inline int rt_backend_shader_modules_ready() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::shader_modules_ready();
    case RTBackendKind::Raylib:
    default:
        return 0;
    }
}

inline int rt_backend_pipeline_layout_ready() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::pipeline_layout_ready();
    case RTBackendKind::Raylib:
    default:
        return 0;
    }
}

inline int rt_backend_texture_sampler_ready() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::texture_sampler_ready();
    case RTBackendKind::Raylib:
    default:
        return 0;
    }
}

inline int rt_backend_texture_image_ready() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::texture_image_ready();
    case RTBackendKind::Raylib:
    default:
        return 0;
    }
}

inline int rt_backend_descriptor_set_ready() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::descriptor_set_ready();
    case RTBackendKind::Raylib:
    default:
        return 0;
    }
}

inline int rt_backend_graphics_pipeline_ready() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::graphics_pipeline_ready();
    case RTBackendKind::Raylib:
    default:
        return 0;
    }
}

inline int rt_backend_command_pool_ready() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::command_pool_ready();
    case RTBackendKind::Raylib:
    default:
        return 0;
    }
}

inline int rt_backend_command_buffer_count() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::command_buffer_count();
    case RTBackendKind::Raylib:
    default:
        return 0;
    }
}

inline int rt_backend_sync_ready() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::sync_ready();
    case RTBackendKind::Raylib:
    default:
        return 0;
    }
}

inline int rt_backend_frame_path_ready() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::frame_path_ready();
    case RTBackendKind::Raylib:
    default:
        return 0;
    }
}

inline int rt_backend_frame_acquired() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::frame_acquired();
    case RTBackendKind::Raylib:
    default:
        return 0;
    }
}

inline int rt_backend_presented_frame_count() {
    switch (rte::backendState.activeKind) {
    case RTBackendKind::Vulkan:
        return vulkan_backend::presented_frame_count();
    case RTBackendKind::Raylib:
    default:
        return 0;
    }
}

inline int rt_backend_supports_3d() {
    if (rte::backendState.activeKind == RTBackendKind::Vulkan) {
        return vulkan_backend::graphics_pipeline_ready() ? 1 : 0;
    }
    return rt_backend_capabilities(rte::backendState.activeKind).supports3D ? 1 : 0;
}

inline int rt_backend_is_available() {
    if (rte::backendState.activeKind == RTBackendKind::Vulkan) {
        return vulkan_backend::runtime_available() ? 1 : 0;
    }
#if !RAYQUIRO_HAS_RAYLIB
    if (rte::backendState.activeKind == RTBackendKind::Raylib) {
        return 0;
    }
#endif
    return rt_backend_capabilities(rte::backendState.activeKind).available ? 1 : 0;
}

inline int rt_backend_is_placeholder() {
    if (rte::backendState.activeKind == RTBackendKind::Vulkan) {
        return vulkan_backend::graphics_pipeline_ready() ? 0 : 1;
    }
    return rt_backend_capabilities(rte::backendState.activeKind).placeholder ? 1 : 0;
}

inline int rt_backend_is_vulkan_family() {
    return rt_backend_capabilities(rte::backendState.activeKind).isVulkanFamily ? 1 : 0;
}
