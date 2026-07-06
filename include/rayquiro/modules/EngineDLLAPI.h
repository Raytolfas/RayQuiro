#pragma once

#ifdef _WIN32
#define RQENGINE_EXPORT extern "C" __declspec(dllexport)
#else
#define RQENGINE_EXPORT extern "C"
#endif

struct RQEngineHandle;

RQENGINE_EXPORT const char* rqengine_version(void);
RQENGINE_EXPORT int rqengine_create(const char* project_root, RQEngineHandle** out_handle, char** out_error);
RQENGINE_EXPORT void rqengine_destroy(RQEngineHandle* handle);

RQENGINE_EXPORT int rqengine_set_backend(RQEngineHandle* handle, const char* backend_name, char** out_error);
RQENGINE_EXPORT int rqengine_set_assets_root(RQEngineHandle* handle, const char* assets_root, char** out_error);
RQENGINE_EXPORT int rqengine_open_window(RQEngineHandle* handle, int width, int height, const char* title, char** out_error);
RQENGINE_EXPORT int rqengine_open_embedded_viewport(
    RQEngineHandle* handle,
    void* parent_window,
    int x,
    int y,
    int width,
    int height,
    const char* title,
    char** out_error);
RQENGINE_EXPORT int rqengine_resize_embedded_viewport(
    RQEngineHandle* handle,
    int x,
    int y,
    int width,
    int height,
    char** out_error);
RQENGINE_EXPORT int rqengine_close_window(RQEngineHandle* handle, char** out_error);
RQENGINE_EXPORT int rqengine_should_close(RQEngineHandle* handle, int* out_should_close, char** out_error);
RQENGINE_EXPORT int rqengine_set_camera(
    RQEngineHandle* handle,
    float px,
    float py,
    float pz,
    float tx,
    float ty,
    float tz,
    float ux,
    float uy,
    float uz,
    float fov,
    char** out_error);
RQENGINE_EXPORT int rqengine_begin_frame(RQEngineHandle* handle, int r, int g, int b, int a, char** out_error);
RQENGINE_EXPORT int rqengine_end_frame(RQEngineHandle* handle, char** out_error);
RQENGINE_EXPORT int rqengine_draw_scene(RQEngineHandle* handle, const char* scene_name, int* out_submitted, char** out_error);
RQENGINE_EXPORT int rqengine_render_stats_json(RQEngineHandle* handle, char** out_json, char** out_error);

RQENGINE_EXPORT int rqengine_select_scene(RQEngineHandle* handle, const char* scene_name, char** out_error);
RQENGINE_EXPORT int rqengine_clear_scene(RQEngineHandle* handle, const char* scene_name, char** out_error);

RQENGINE_EXPORT int rqengine_upsert_entity(
    RQEngineHandle* handle,
    const char* scene_name,
    const char* entity_name,
    const char* kind,
    char** out_error);

RQENGINE_EXPORT int rqengine_set_entity_position(
    RQEngineHandle* handle,
    const char* scene_name,
    const char* entity_name,
    float x,
    float y,
    float z,
    char** out_error);

RQENGINE_EXPORT int rqengine_set_entity_scale(
    RQEngineHandle* handle,
    const char* scene_name,
    const char* entity_name,
    float x,
    float y,
    float z,
    char** out_error);

RQENGINE_EXPORT int rqengine_set_entity_color(
    RQEngineHandle* handle,
    const char* scene_name,
    const char* entity_name,
    int r,
    int g,
    int b,
    int a,
    char** out_error);

RQENGINE_EXPORT int rqengine_bind_entity_resources(
    RQEngineHandle* handle,
    const char* scene_name,
    const char* entity_name,
    const char* mesh_name,
    const char* material_name,
    const char* texture_name,
    char** out_error);

RQENGINE_EXPORT int rqengine_upsert_mesh(
    RQEngineHandle* handle,
    const char* scene_name,
    const char* mesh_name,
    const char* primitive,
    float sx,
    float sy,
    float sz,
    const char* source,
    char** out_error);

RQENGINE_EXPORT int rqengine_upsert_material(
    RQEngineHandle* handle,
    const char* scene_name,
    const char* material_name,
    int r,
    int g,
    int b,
    int a,
    float roughness,
    float metallic,
    int er,
    int eg,
    int eb,
    int ea,
    const char* texture_name,
    char** out_error);

RQENGINE_EXPORT int rqengine_upsert_texture(
    RQEngineHandle* handle,
    const char* scene_name,
    const char* texture_name,
    const char* source,
    int srgb,
    int normal_map,
    char** out_error);

RQENGINE_EXPORT int rqengine_set_ambient_light(
    RQEngineHandle* handle,
    const char* scene_name,
    int r,
    int g,
    int b,
    int a,
    char** out_error);

RQENGINE_EXPORT int rqengine_set_directional_light(
    RQEngineHandle* handle,
    const char* scene_name,
    float dx,
    float dy,
    float dz,
    int r,
    int g,
    int b,
    int a,
    float intensity,
    char** out_error);

RQENGINE_EXPORT int rqengine_backend_info_json(RQEngineHandle* handle, char** out_json, char** out_error);
RQENGINE_EXPORT int rqengine_scene_stats_json(RQEngineHandle* handle, const char* scene_name, char** out_json, char** out_error);
RQENGINE_EXPORT int rqengine_export_scene_json(RQEngineHandle* handle, const char* scene_name, char** out_json, char** out_error);
RQENGINE_EXPORT int rqengine_save_scene(RQEngineHandle* handle, const char* scene_name, const char* file_path, char** out_path, char** out_error);
RQENGINE_EXPORT int rqengine_load_scene(RQEngineHandle* handle, const char* scene_name, const char* file_path, char** out_path, char** out_error);
RQENGINE_EXPORT void rqengine_free_string(char* value);
