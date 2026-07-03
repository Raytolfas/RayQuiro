#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_TGA
#include "../../third_party/raylib/src/external/stb_image.h"

#include "../../include/rayquiro/EngineBackend.h"
#include "../../include/rayquiro/EngineDLLAPI.h"
#include "../../include/rayquiro/NativeModuleABI.h"
#include "../../include/rayquiro/rte_api.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

struct RQEngineHandle;

namespace {
struct EngineVec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct EngineColor {
    int r = 255;
    int g = 255;
    int b = 255;
    int a = 255;
};

struct EngineLight {
    EngineVec3 direction {0.3f, -1.0f, 0.2f};
    EngineColor color {255, 244, 214, 255};
    float intensity = 1.0f;
};

struct EngineMaterial {
    EngineColor albedo {255, 255, 255, 255};
    EngineColor emissive {0, 0, 0, 255};
    std::string texture;
    float roughness = 0.5f;
    float metallic = 0.0f;
};

struct EngineMesh {
    std::string primitive = "cube";
    std::string source;
    EngineVec3 defaultSize {1.0f, 1.0f, 1.0f};
    float defaultRadius = 1.0f;
    float defaultPlaneWidth = 4.0f;
    float defaultPlaneHeight = 4.0f;
};

struct EngineTexture {
    std::string source;
    bool srgb = true;
    bool normalMap = false;
    bool loadAttempted = false;
    int width = 0;
    int height = 0;
    std::vector<unsigned char> rgba;
    std::string resolvedPath;
    std::string loadError;
};

struct EngineEntity {
    std::string kind = "cube";
    EngineVec3 position {0.0f, 0.5f, 0.0f};
    EngineVec3 scale {1.0f, 1.0f, 1.0f};
    EngineColor color {255, 255, 255, 255};
    bool visible = true;
    std::string mesh;
    std::string material;
    std::string texture;
};

struct EngineScene {
    std::unordered_map<std::string, EngineEntity> entities;
    std::unordered_map<std::string, EngineMaterial> materials;
    std::unordered_map<std::string, EngineMesh> meshes;
    std::unordered_map<std::string, EngineTexture> textures;
    EngineColor ambient {30, 38, 54, 255};
    EngineLight sun;
    bool hasSun = false;
};

struct EnginePostFX {
    float exposure = 1.0f;
    float vignette = 0.08f;
    float filmGrain = 0.02f;
    float saturation = 1.0f;
    float contrast = 1.05f;
    float bloom = 0.08f;
    EngineColor fogColor {16, 20, 32, 255};
    float fogNear = 10.0f;
    float fogFar = 48.0f;
    float fogDensity = 0.12f;
    float volumetric = 0.18f;
};

struct EngineSceneWatch {
    bool enabled = false;
    std::filesystem::path path;
    std::string sceneName;
    std::filesystem::file_time_type lastWrite{};
};

struct EngineStore {
    std::filesystem::path projectRoot;
    std::string assetsRoot = "assets";
    std::string currentScene = "main";
    RTBackendKind requestedBackend = RTBackendKind::Raylib;
    EngineVec3 cameraPosition {6.0f, 6.0f, 6.0f};
    EngineVec3 cameraTarget {0.0f, 0.0f, 0.0f};
    EngineVec3 cameraUp {0.0f, 1.0f, 0.0f};
    float cameraFov = 45.0f;
    EnginePostFX postfx;
    EngineSceneWatch sceneWatch;
    std::unordered_map<std::string, EngineScene> scenes;
};

struct ScalarValue {
    std::variant<std::monostate, double, std::string, bool> data;

    ScalarValue() : data(std::monostate{}) {}
    ScalarValue(double value) : data(value) {}
    ScalarValue(std::string value) : data(std::move(value)) {}
    ScalarValue(bool value) : data(value) {}
};

struct JsonNode {
    enum class Type {
        Null,
        Number,
        String,
        Bool,
        Array,
        Object
    };

    Type type = Type::Null;
    double number = 0.0;
    std::string string;
    bool boolean = false;
    std::vector<JsonNode> array;
    std::unordered_map<std::string, JsonNode> object;
};

EngineStore& module_store() {
    static EngineStore store;
    return store;
}

char* duplicate_string(const std::string& value) {
    char* memory = new char[value.size() + 1];
    std::memcpy(memory, value.c_str(), value.size() + 1);
    return memory;
}

void assign_error(char** outError, const std::string& message) {
    if (outError != nullptr) {
        *outError = duplicate_string(message);
    }
}

void clear_error(char** outError) {
    if (outError != nullptr) {
        *outError = nullptr;
    }
}

std::string trim(const std::string& value) {
    const auto start = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch) != 0; });
    const auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch) != 0; }).base();
    if (start >= end) return "";
    return std::string(start, end);
}

std::string json_escape(const std::string& value) {
    std::string result;
    result.reserve(value.size() + 8);
    for (char ch : value) {
        switch (ch) {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default: result += ch; break;
        }
    }
    return result;
}

std::string json_bool(bool value) {
    return value ? "true" : "false";
}

std::string json_number(double value) {
    std::ostringstream out;
    out << value;
    return out.str();
}

void json_skip_ws(const std::string& text, size_t& pos) {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])) != 0) {
        ++pos;
    }
}

std::string json_parse_string(const std::string& text, size_t& pos) {
    if (pos >= text.size() || text[pos] != '"') {
        throw std::runtime_error("Invalid JSON string.");
    }
    ++pos;
    std::string result;
    while (pos < text.size()) {
        const char ch = text[pos++];
        if (ch == '"') return result;
        if (ch == '\\') {
            if (pos >= text.size()) break;
            const char escaped = text[pos++];
            if (escaped == 'n') result += '\n';
            else if (escaped == 'r') result += '\r';
            else if (escaped == 't') result += '\t';
            else result += escaped;
            continue;
        }
        result += ch;
    }
    throw std::runtime_error("Unterminated JSON string.");
}

ScalarValue json_parse_scalar(const std::string& text, size_t& pos) {
    json_skip_ws(text, pos);
    if (pos >= text.size()) {
        throw std::runtime_error("Unexpected end of JSON.");
    }
    if (text[pos] == '"') {
        return ScalarValue(json_parse_string(text, pos));
    }
    if (text.compare(pos, 4, "true") == 0) {
        pos += 4;
        return ScalarValue(true);
    }
    if (text.compare(pos, 5, "false") == 0) {
        pos += 5;
        return ScalarValue(false);
    }
    if (text.compare(pos, 4, "null") == 0) {
        pos += 4;
        return ScalarValue();
    }

    size_t end = pos;
    while (end < text.size()) {
        const char ch = text[end];
        if (!(std::isdigit(static_cast<unsigned char>(ch)) != 0 || ch == '-' || ch == '+' || ch == '.' || ch == 'e' || ch == 'E')) {
            break;
        }
        ++end;
    }
    if (end == pos) {
        throw std::runtime_error("Invalid JSON scalar.");
    }

    const double number = std::stod(text.substr(pos, end - pos));
    pos = end;
    return ScalarValue(number);
}

std::string json_capture_compound(const std::string& text, size_t& pos, char openCh, char closeCh) {
    const size_t start = pos;
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    while (pos < text.size()) {
        const char ch = text[pos++];
        if (inString) {
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == '"') inString = false;
            continue;
        }
        if (ch == '"') {
            inString = true;
            continue;
        }
        if (ch == openCh) {
            ++depth;
            continue;
        }
        if (ch == closeCh) {
            --depth;
            if (depth == 0) {
                return text.substr(start, pos - start);
            }
        }
    }
    throw std::runtime_error("Unterminated JSON compound value.");
}

JsonNode json_parse_node(const std::string& text, size_t& pos);

JsonNode json_parse_array_node(const std::string& text, size_t& pos) {
    if (pos >= text.size() || text[pos] != '[') {
        throw std::runtime_error("Invalid JSON array.");
    }
    ++pos;
    JsonNode node;
    node.type = JsonNode::Type::Array;
    json_skip_ws(text, pos);
    if (pos < text.size() && text[pos] == ']') {
        ++pos;
        return node;
    }
    while (pos < text.size()) {
        node.array.push_back(json_parse_node(text, pos));
        json_skip_ws(text, pos);
        if (pos < text.size() && text[pos] == ',') {
            ++pos;
            continue;
        }
        if (pos < text.size() && text[pos] == ']') {
            ++pos;
            return node;
        }
        throw std::runtime_error("Invalid JSON array value.");
    }
    throw std::runtime_error("Unterminated JSON array.");
}

JsonNode json_parse_object_node(const std::string& text, size_t& pos) {
    if (pos >= text.size() || text[pos] != '{') {
        throw std::runtime_error("Invalid JSON object.");
    }
    ++pos;
    JsonNode node;
    node.type = JsonNode::Type::Object;
    json_skip_ws(text, pos);
    if (pos < text.size() && text[pos] == '}') {
        ++pos;
        return node;
    }
    while (pos < text.size()) {
        json_skip_ws(text, pos);
        const std::string key = json_parse_string(text, pos);
        json_skip_ws(text, pos);
        if (pos >= text.size() || text[pos] != ':') {
            throw std::runtime_error("Invalid JSON object separator.");
        }
        ++pos;
        node.object[key] = json_parse_node(text, pos);
        json_skip_ws(text, pos);
        if (pos < text.size() && text[pos] == ',') {
            ++pos;
            continue;
        }
        if (pos < text.size() && text[pos] == '}') {
            ++pos;
            return node;
        }
        throw std::runtime_error("Invalid JSON object value.");
    }
    throw std::runtime_error("Unterminated JSON object.");
}

JsonNode json_parse_node(const std::string& text, size_t& pos) {
    json_skip_ws(text, pos);
    if (pos >= text.size()) {
        throw std::runtime_error("Unexpected end of JSON.");
    }
    if (text[pos] == '{') {
        return json_parse_object_node(text, pos);
    }
    if (text[pos] == '[') {
        return json_parse_array_node(text, pos);
    }
    if (text[pos] == '"') {
        JsonNode node;
        node.type = JsonNode::Type::String;
        node.string = json_parse_string(text, pos);
        return node;
    }
    if (text.compare(pos, 4, "true") == 0) {
        pos += 4;
        JsonNode node;
        node.type = JsonNode::Type::Bool;
        node.boolean = true;
        return node;
    }
    if (text.compare(pos, 5, "false") == 0) {
        pos += 5;
        JsonNode node;
        node.type = JsonNode::Type::Bool;
        node.boolean = false;
        return node;
    }
    if (text.compare(pos, 4, "null") == 0) {
        pos += 4;
        return JsonNode{};
    }

    JsonNode node;
    node.type = JsonNode::Type::Number;
    node.number = std::get<double>(json_parse_scalar(text, pos).data);
    return node;
}

JsonNode json_parse_document(const std::string& text) {
    size_t pos = 0;
    JsonNode root = json_parse_node(text, pos);
    json_skip_ws(text, pos);
    if (pos != text.size()) {
        throw std::runtime_error("Unexpected trailing JSON data.");
    }
    return root;
}

std::vector<ScalarValue> parse_args(const char* jsonArgs) {
    const std::string text = jsonArgs ? jsonArgs : "[]";
    size_t pos = 0;
    json_skip_ws(text, pos);
    if (pos >= text.size() || text[pos] != '[') {
        throw std::runtime_error("Native module args must be a JSON array.");
    }
    ++pos;

    std::vector<ScalarValue> result;
    json_skip_ws(text, pos);
    if (pos < text.size() && text[pos] == ']') {
        ++pos;
        return result;
    }

    while (pos < text.size()) {
        json_skip_ws(text, pos);
        if (pos < text.size() && text[pos] == '[') {
            result.push_back(ScalarValue(json_capture_compound(text, pos, '[', ']')));
        } else if (pos < text.size() && text[pos] == '{') {
            result.push_back(ScalarValue(json_capture_compound(text, pos, '{', '}')));
        } else {
            result.push_back(json_parse_scalar(text, pos));
        }
        json_skip_ws(text, pos);
        if (pos < text.size() && text[pos] == ',') {
            ++pos;
            continue;
        }
        if (pos < text.size() && text[pos] == ']') {
            ++pos;
            break;
        }
        throw std::runtime_error("Invalid JSON array payload.");
    }
    return result;
}

std::string to_string(const ScalarValue& value) {
    if (std::holds_alternative<std::monostate>(value.data)) return "";
    if (std::holds_alternative<std::string>(value.data)) return std::get<std::string>(value.data);
    if (std::holds_alternative<bool>(value.data)) return std::get<bool>(value.data) ? "true" : "false";
    return json_number(std::get<double>(value.data));
}

double to_number(const ScalarValue& value) {
    if (std::holds_alternative<double>(value.data)) return std::get<double>(value.data);
    if (std::holds_alternative<bool>(value.data)) return std::get<bool>(value.data) ? 1.0 : 0.0;
    if (std::holds_alternative<std::string>(value.data)) {
        try {
            return std::stod(std::get<std::string>(value.data));
        } catch (...) {
            return 0.0;
        }
    }
    return 0.0;
}

bool to_bool(const ScalarValue& value, bool fallback = false) {
    if (std::holds_alternative<bool>(value.data)) return std::get<bool>(value.data);
    if (std::holds_alternative<double>(value.data)) return std::get<double>(value.data) != 0.0;
    if (std::holds_alternative<std::string>(value.data)) {
        const std::string text = trim(std::get<std::string>(value.data));
        if (text == "true" || text == "1") return true;
        if (text == "false" || text == "0") return false;
    }
    return fallback;
}

EngineVec3 vec3_sub(const EngineVec3& a, const EngineVec3& b) {
    return EngineVec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

float vec3_dot(const EngineVec3& a, const EngineVec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float vec3_length(const EngineVec3& value) {
    return std::sqrt(vec3_dot(value, value));
}

EngineVec3 vec3_normalize(const EngineVec3& value) {
    const float length = vec3_length(value);
    if (length <= 0.0001f) {
        return EngineVec3{0.0f, 0.0f, -1.0f};
    }
    return EngineVec3{value.x / length, value.y / length, value.z / length};
}

std::vector<double> parse_number_array_text(const std::string& text) {
    std::vector<double> values;
    const std::string trimmed = trim(text);
    if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']') {
        return values;
    }
    size_t pos = 1;
    while (pos + 1 < trimmed.size()) {
        while (pos < trimmed.size() && (std::isspace(static_cast<unsigned char>(trimmed[pos])) != 0 || trimmed[pos] == ',')) ++pos;
        if (pos >= trimmed.size() || trimmed[pos] == ']') break;
        size_t end = pos;
        while (end < trimmed.size()) {
            const char ch = trimmed[end];
            if (!(std::isdigit(static_cast<unsigned char>(ch)) != 0 || ch == '-' || ch == '+' || ch == '.' || ch == 'e' || ch == 'E')) break;
            ++end;
        }
        if (end == pos) break;
        values.push_back(std::stod(trimmed.substr(pos, end - pos)));
        pos = end;
    }
    return values;
}

EngineVec3 value_to_vec3(const ScalarValue& value, EngineVec3 fallback = EngineVec3{0.0f, 0.0f, 0.0f}) {
    if (std::holds_alternative<std::string>(value.data)) {
        const auto values = parse_number_array_text(std::get<std::string>(value.data));
        if (values.size() >= 3) {
            return EngineVec3{static_cast<float>(values[0]), static_cast<float>(values[1]), static_cast<float>(values[2])};
        }
    }
    if (std::holds_alternative<double>(value.data)) {
        const float number = static_cast<float>(std::get<double>(value.data));
        return EngineVec3{number, number, number};
    }
    return fallback;
}

EngineColor value_to_color(const ScalarValue& value, EngineColor fallback = EngineColor{255, 255, 255, 255}) {
    if (std::holds_alternative<std::string>(value.data)) {
        const auto values = parse_number_array_text(std::get<std::string>(value.data));
        if (values.size() >= 3) {
            return EngineColor{
                static_cast<int>(values[0]),
                static_cast<int>(values[1]),
                static_cast<int>(values[2]),
                values.size() > 3 ? static_cast<int>(values[3]) : 255
            };
        }
    }
    return fallback;
}

RTVec3 to_rt_vec3(const EngineVec3& value) {
    return RTVec3{value.x, value.y, value.z};
}

RTColor to_rt_color(const EngineColor& value) {
    return RTColor{
        static_cast<unsigned char>(std::clamp(value.r, 0, 255)),
        static_cast<unsigned char>(std::clamp(value.g, 0, 255)),
        static_cast<unsigned char>(std::clamp(value.b, 0, 255)),
        static_cast<unsigned char>(std::clamp(value.a, 0, 255))
    };
}

EngineColor mix_color(const EngineColor& base, const EngineColor& overlay, float factor) {
    const float t = std::clamp(factor, 0.0f, 1.0f);
    auto mix_channel = [t](int a, int b) -> int {
        return static_cast<int>(std::clamp(a * (1.0f - t) + b * t, 0.0f, 255.0f));
    };
    return EngineColor{
        mix_channel(base.r, overlay.r),
        mix_channel(base.g, overlay.g),
        mix_channel(base.b, overlay.b),
        base.a
    };
}

EngineColor shade_with_light(const EngineColor& base, const EngineColor& ambient, const EngineLight* light) {
    EngineColor shaded = mix_color(base, ambient, 0.25f);
    if (light != nullptr) {
        shaded = mix_color(shaded, light->color, std::clamp(0.15f * light->intensity, 0.0f, 0.45f));
    }
    return shaded;
}

std::string require_name(const std::string& value, const char* label) {
    const std::string clean = trim(value);
    if (clean.empty()) {
        throw std::runtime_error(std::string(label) + " cannot be empty.");
    }
    return clean;
}

std::filesystem::path resolve_texture_source_path(const EngineStore& store, const std::string& source) {
    const std::string clean = trim(source);
    if (clean.empty()) {
        return {};
    }

    const std::filesystem::path direct(clean);
    if (direct.is_absolute()) {
        return direct;
    }

    const std::array<std::filesystem::path, 5> candidates = {{
        std::filesystem::path(store.assetsRoot) / clean,
        std::filesystem::path(store.assetsRoot) / "textures" / clean,
        std::filesystem::path("assets") / clean,
        std::filesystem::path("assets") / "textures" / clean,
        direct
    }};

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    return std::filesystem::path(store.assetsRoot) / clean;
}

bool ensure_texture_rgba_loaded(const EngineStore& store, EngineTexture& texture) {
    if (texture.loadAttempted) {
        return !texture.rgba.empty() && texture.width > 0 && texture.height > 0;
    }

    texture.loadAttempted = true;
    texture.resolvedPath.clear();
    texture.loadError.clear();
    texture.rgba.clear();
    texture.width = 0;
    texture.height = 0;

    const std::filesystem::path resolved = resolve_texture_source_path(store, texture.source);
    texture.resolvedPath = resolved.string();
    if (resolved.empty() || !std::filesystem::exists(resolved)) {
        texture.loadError = "Texture source was not found.";
        return false;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load(texture.resolvedPath.c_str(), &width, &height, &channels, 4);
    if (pixels == nullptr || width <= 0 || height <= 0) {
        texture.loadError = stbi_failure_reason() == nullptr ? "stbi_load failed." : stbi_failure_reason();
        if (pixels != nullptr) {
            stbi_image_free(pixels);
        }
        return false;
    }

    texture.width = width;
    texture.height = height;
    texture.rgba.assign(pixels, pixels + static_cast<size_t>(width * height * 4));
    stbi_image_free(pixels);
    return true;
}

EngineScene& ensure_scene(EngineStore& store, const std::string& requestedName) {
    const std::string name = require_name(requestedName.empty() ? store.currentScene : requestedName, "scene");
    store.currentScene = name;
    auto found = store.scenes.find(name);
    if (found == store.scenes.end()) {
        found = store.scenes.emplace(name, EngineScene{}).first;
    }
    return found->second;
}

EngineScene& current_scene(EngineStore& store) {
    return ensure_scene(store, store.currentScene);
}

EngineEntity& ensure_entity(EngineStore& store, const std::string& sceneName, const std::string& entityName, const std::string& kind) {
    EngineScene& scene = ensure_scene(store, sceneName);
    const std::string name = require_name(entityName, "entity");
    auto found = scene.entities.find(name);
    if (found == scene.entities.end()) {
        EngineEntity entity;
        if (!trim(kind).empty()) {
            entity.kind = trim(kind);
        }
        found = scene.entities.emplace(name, entity).first;
    } else if (!trim(kind).empty()) {
        found->second.kind = trim(kind);
    }
    return found->second;
}

std::string backend_info_json(const EngineStore& store) {
    const RTBackendCapabilities capabilities = rt_backend_capabilities(store.requestedBackend);
    const bool available = rt_backend_is_available() != 0;
    const bool supports3d = rt_backend_supports_3d() != 0;
    const bool placeholder = rt_backend_is_placeholder() != 0;
    std::ostringstream out;
    out << "{";
    out << "\"requested\":\"" << json_escape(rt_backend_name_from_kind(store.requestedBackend)) << "\",";
    out << "\"active\":\"" << json_escape(rt_backend_name()) << "\",";
    out << "\"available\":" << json_bool(available) << ",";
    out << "\"supports3d\":" << json_bool(supports3d) << ",";
    out << "\"placeholder\":" << json_bool(placeholder) << ",";
    out << "\"vulkan_family\":" << json_bool(capabilities.isVulkanFamily) << ",";
    out << "\"scene_owner\":\"engine.dll\",";
    out << "\"editor_owns_scene\":false,";
    out << "\"assets_root\":\"" << json_escape(store.assetsRoot) << "\",";
    out << "\"gpu_count\":" << rt_backend_gpu_count() << ",";
    out << "\"surface_ready\":" << json_bool(rt_backend_surface_ready() != 0) << ",";
    out << "\"device_ready\":" << json_bool(rt_backend_device_ready() != 0) << ",";
    out << "\"swapchain_ready\":" << json_bool(rt_backend_swapchain_ready() != 0) << ",";
    out << "\"render_pass_ready\":" << json_bool(rt_backend_render_pass_ready() != 0) << ",";
    out << "\"depth_ready\":" << json_bool(rt_backend_depth_ready() != 0) << ",";
    out << "\"texture_sampler_ready\":" << json_bool(rt_backend_texture_sampler_ready() != 0) << ",";
    out << "\"texture_image_ready\":" << json_bool(rt_backend_texture_image_ready() != 0) << ",";
    out << "\"descriptor_set_ready\":" << json_bool(rt_backend_descriptor_set_ready() != 0) << ",";
    out << "\"graphics_pipeline_ready\":" << json_bool(rt_backend_graphics_pipeline_ready() != 0) << ",";
    out << "\"presented_frames\":" << rt_backend_presented_frame_count();
    out << "}";
    return out.str();
}

std::string entity_json(const EngineEntity& entity) {
    std::ostringstream out;
    out << "{";
    out << "\"kind\":\"" << json_escape(entity.kind) << "\",";
    out << "\"visible\":" << json_bool(entity.visible) << ",";
    out << "\"position\":[" << entity.position.x << "," << entity.position.y << "," << entity.position.z << "],";
    out << "\"scale\":[" << entity.scale.x << "," << entity.scale.y << "," << entity.scale.z << "],";
    out << "\"color\":[" << entity.color.r << "," << entity.color.g << "," << entity.color.b << "," << entity.color.a << "],";
    out << "\"mesh\":\"" << json_escape(entity.mesh) << "\",";
    out << "\"material\":\"" << json_escape(entity.material) << "\",";
    out << "\"texture\":\"" << json_escape(entity.texture) << "\"";
    out << "}";
    return out.str();
}

std::string scene_stats_json(const EngineStore& store, const std::string& sceneName) {
    const auto found = store.scenes.find(trim(sceneName).empty() ? store.currentScene : trim(sceneName));
    const EngineScene emptyScene;
    const EngineScene& scene = (found == store.scenes.end()) ? emptyScene : found->second;
    std::ostringstream out;
    out << "{";
    out << "\"scene\":\"" << json_escape(trim(sceneName).empty() ? store.currentScene : trim(sceneName)) << "\",";
    out << "\"entity_count\":" << scene.entities.size() << ",";
    out << "\"material_count\":" << scene.materials.size() << ",";
    out << "\"mesh_count\":" << scene.meshes.size() << ",";
    out << "\"texture_count\":" << scene.textures.size();
    out << "}";
    return out.str();
}

std::string vec3_json(const EngineVec3& value) {
    std::ostringstream out;
    out << "[" << value.x << "," << value.y << "," << value.z << "]";
    return out.str();
}

std::string color_json(const EngineColor& value) {
    std::ostringstream out;
    out << "[" << value.r << "," << value.g << "," << value.b << "," << value.a << "]";
    return out.str();
}

std::string material_json(const EngineMaterial& material) {
    std::ostringstream out;
    out << "{";
    out << "\"albedo\":" << color_json(material.albedo) << ",";
    out << "\"emissive\":" << color_json(material.emissive) << ",";
    out << "\"texture\":\"" << json_escape(material.texture) << "\",";
    out << "\"roughness\":" << material.roughness << ",";
    out << "\"metallic\":" << material.metallic;
    out << "}";
    return out.str();
}

std::string mesh_json(const EngineMesh& mesh) {
    std::ostringstream out;
    out << "{";
    out << "\"primitive\":\"" << json_escape(mesh.primitive) << "\",";
    out << "\"source\":\"" << json_escape(mesh.source) << "\",";
    out << "\"default_size\":" << vec3_json(mesh.defaultSize) << ",";
    out << "\"default_radius\":" << mesh.defaultRadius << ",";
    out << "\"default_plane_size\":[" << mesh.defaultPlaneWidth << "," << mesh.defaultPlaneHeight << "]";
    out << "}";
    return out.str();
}

std::string texture_json(const EngineTexture& texture) {
    std::ostringstream out;
    out << "{";
    out << "\"source\":\"" << json_escape(texture.source) << "\",";
    out << "\"srgb\":" << json_bool(texture.srgb) << ",";
    out << "\"normal_map\":" << json_bool(texture.normalMap);
    out << "}";
    return out.str();
}

std::filesystem::path resolve_scene_path(const EngineStore& store, const std::string& requestedPath, const std::string& fallbackSceneName) {
    std::filesystem::path path = trim(requestedPath).empty()
        ? (store.projectRoot / "scenes" / (fallbackSceneName + ".scene"))
        : std::filesystem::path(requestedPath);
    if (!path.is_absolute()) {
        path = store.projectRoot / path;
    }
    if (path.extension() != ".scene") {
        path.replace_extension(".scene");
    }
    return path.lexically_normal();
}

const JsonNode* json_object_member(const JsonNode& node, const std::string& key) {
    if (node.type != JsonNode::Type::Object) {
        return nullptr;
    }
    const auto found = node.object.find(key);
    return found == node.object.end() ? nullptr : &found->second;
}

std::string json_node_string(const JsonNode* node, const std::string& fallback = "") {
    return (node != nullptr && node->type == JsonNode::Type::String) ? node->string : fallback;
}

double json_node_number(const JsonNode* node, double fallback = 0.0) {
    return (node != nullptr && node->type == JsonNode::Type::Number) ? node->number : fallback;
}

bool json_node_bool(const JsonNode* node, bool fallback = false) {
    return (node != nullptr && node->type == JsonNode::Type::Bool) ? node->boolean : fallback;
}

EngineVec3 json_node_vec3(const JsonNode* node, EngineVec3 fallback = EngineVec3{0.0f, 0.0f, 0.0f}) {
    if (node == nullptr || node->type != JsonNode::Type::Array || node->array.size() < 3) {
        return fallback;
    }
    return EngineVec3{
        static_cast<float>(json_node_number(&node->array[0], fallback.x)),
        static_cast<float>(json_node_number(&node->array[1], fallback.y)),
        static_cast<float>(json_node_number(&node->array[2], fallback.z))
    };
}

EngineColor json_node_color(const JsonNode* node, EngineColor fallback = EngineColor{255, 255, 255, 255}) {
    if (node == nullptr || node->type != JsonNode::Type::Array || node->array.size() < 4) {
        return fallback;
    }
    return EngineColor{
        static_cast<int>(json_node_number(&node->array[0], fallback.r)),
        static_cast<int>(json_node_number(&node->array[1], fallback.g)),
        static_cast<int>(json_node_number(&node->array[2], fallback.b)),
        static_cast<int>(json_node_number(&node->array[3], fallback.a))
    };
}

std::string export_scene_json(const EngineStore& store, const std::string& sceneName) {
    const std::string resolved = trim(sceneName).empty() ? store.currentScene : trim(sceneName);
    const auto found = store.scenes.find(resolved);
    const EngineScene emptyScene;
    const EngineScene& scene = (found == store.scenes.end()) ? emptyScene : found->second;

    std::ostringstream out;
    out << "{";
    out << "\"scene\":\"" << json_escape(resolved) << "\",";
    out << "\"assets_root\":\"" << json_escape(store.assetsRoot) << "\",";
    out << "\"camera\":{";
    out << "\"position\":" << vec3_json(store.cameraPosition) << ",";
    out << "\"target\":" << vec3_json(store.cameraTarget) << ",";
    out << "\"up\":" << vec3_json(store.cameraUp) << ",";
    out << "\"fov\":" << store.cameraFov;
    out << "},";
    out << "\"ambient\":" << color_json(scene.ambient) << ",";
    out << "\"has_sun\":" << json_bool(scene.hasSun) << ",";
    out << "\"sun\":{";
    out << "\"direction\":" << vec3_json(scene.sun.direction) << ",";
    out << "\"color\":" << color_json(scene.sun.color) << ",";
    out << "\"intensity\":" << scene.sun.intensity;
    out << "},";
    out << "\"backend\":" << backend_info_json(store) << ",";
    out << "\"materials\":{";
    bool firstMaterial = true;
    for (const auto& pair : scene.materials) {
        if (!firstMaterial) out << ",";
        firstMaterial = false;
        out << "\"" << json_escape(pair.first) << "\":" << material_json(pair.second);
    }
    out << "},";
    out << "\"meshes\":{";
    bool firstMesh = true;
    for (const auto& pair : scene.meshes) {
        if (!firstMesh) out << ",";
        firstMesh = false;
        out << "\"" << json_escape(pair.first) << "\":" << mesh_json(pair.second);
    }
    out << "},";
    out << "\"textures\":{";
    bool firstTexture = true;
    for (const auto& pair : scene.textures) {
        if (!firstTexture) out << ",";
        firstTexture = false;
        out << "\"" << json_escape(pair.first) << "\":" << texture_json(pair.second);
    }
    out << "},";
    out << "\"entities\":{";
    bool first = true;
    for (const auto& pair : scene.entities) {
        if (!first) out << ",";
        first = false;
        out << "\"" << json_escape(pair.first) << "\":" << entity_json(pair.second);
    }
    out << "}";
    out << "}";
    return out.str();
}

void import_scene_json(EngineStore& store, const std::string& text, const std::string& explicitSceneName = "") {
    const JsonNode root = json_parse_document(text);
    if (root.type != JsonNode::Type::Object) {
        throw std::runtime_error("Scene file must be a JSON object.");
    }

    const std::string sceneName = !trim(explicitSceneName).empty()
        ? require_name(explicitSceneName, "scene")
        : require_name(json_node_string(json_object_member(root, "scene"), store.currentScene), "scene");

    EngineScene imported;
    imported.ambient = json_node_color(json_object_member(root, "ambient"), imported.ambient);
    imported.hasSun = json_node_bool(json_object_member(root, "has_sun"), false);
    if (const JsonNode* sunNode = json_object_member(root, "sun")) {
        imported.sun.direction = json_node_vec3(json_object_member(*sunNode, "direction"), imported.sun.direction);
        imported.sun.color = json_node_color(json_object_member(*sunNode, "color"), imported.sun.color);
        imported.sun.intensity = static_cast<float>(json_node_number(json_object_member(*sunNode, "intensity"), imported.sun.intensity));
    }

    if (const JsonNode* cameraNode = json_object_member(root, "camera")) {
        store.cameraPosition = json_node_vec3(json_object_member(*cameraNode, "position"), store.cameraPosition);
        store.cameraTarget = json_node_vec3(json_object_member(*cameraNode, "target"), store.cameraTarget);
        store.cameraUp = json_node_vec3(json_object_member(*cameraNode, "up"), store.cameraUp);
        store.cameraFov = static_cast<float>(json_node_number(json_object_member(*cameraNode, "fov"), store.cameraFov));
    }

    if (const JsonNode* assetsRootNode = json_object_member(root, "assets_root")) {
        store.assetsRoot = json_node_string(assetsRootNode, store.assetsRoot);
    }

    if (const JsonNode* materialsNode = json_object_member(root, "materials"); materialsNode != nullptr && materialsNode->type == JsonNode::Type::Object) {
        for (const auto& pair : materialsNode->object) {
            EngineMaterial material;
            material.albedo = json_node_color(json_object_member(pair.second, "albedo"), material.albedo);
            material.emissive = json_node_color(json_object_member(pair.second, "emissive"), material.emissive);
            material.texture = json_node_string(json_object_member(pair.second, "texture"), "");
            material.roughness = static_cast<float>(json_node_number(json_object_member(pair.second, "roughness"), material.roughness));
            material.metallic = static_cast<float>(json_node_number(json_object_member(pair.second, "metallic"), material.metallic));
            imported.materials[pair.first] = material;
        }
    }

    if (const JsonNode* meshesNode = json_object_member(root, "meshes"); meshesNode != nullptr && meshesNode->type == JsonNode::Type::Object) {
        for (const auto& pair : meshesNode->object) {
            EngineMesh mesh;
            mesh.primitive = json_node_string(json_object_member(pair.second, "primitive"), mesh.primitive);
            mesh.source = json_node_string(json_object_member(pair.second, "source"), "");
            mesh.defaultSize = json_node_vec3(json_object_member(pair.second, "default_size"), mesh.defaultSize);
            mesh.defaultRadius = static_cast<float>(json_node_number(json_object_member(pair.second, "default_radius"), mesh.defaultRadius));
            if (const JsonNode* planeNode = json_object_member(pair.second, "default_plane_size"); planeNode != nullptr && planeNode->type == JsonNode::Type::Array && planeNode->array.size() >= 2) {
                mesh.defaultPlaneWidth = static_cast<float>(json_node_number(&planeNode->array[0], mesh.defaultPlaneWidth));
                mesh.defaultPlaneHeight = static_cast<float>(json_node_number(&planeNode->array[1], mesh.defaultPlaneHeight));
            }
            imported.meshes[pair.first] = mesh;
        }
    }

    if (const JsonNode* texturesNode = json_object_member(root, "textures"); texturesNode != nullptr && texturesNode->type == JsonNode::Type::Object) {
        for (const auto& pair : texturesNode->object) {
            EngineTexture texture;
            texture.source = json_node_string(json_object_member(pair.second, "source"), "");
            texture.srgb = json_node_bool(json_object_member(pair.second, "srgb"), true);
            texture.normalMap = json_node_bool(json_object_member(pair.second, "normal_map"), false);
            imported.textures[pair.first] = texture;
        }
    }

    if (const JsonNode* entitiesNode = json_object_member(root, "entities"); entitiesNode != nullptr && entitiesNode->type == JsonNode::Type::Object) {
        for (const auto& pair : entitiesNode->object) {
            EngineEntity entity;
            entity.kind = json_node_string(json_object_member(pair.second, "kind"), entity.kind);
            entity.visible = json_node_bool(json_object_member(pair.second, "visible"), entity.visible);
            entity.position = json_node_vec3(json_object_member(pair.second, "position"), entity.position);
            entity.scale = json_node_vec3(json_object_member(pair.second, "scale"), entity.scale);
            entity.color = json_node_color(json_object_member(pair.second, "color"), entity.color);
            entity.mesh = json_node_string(json_object_member(pair.second, "mesh"), "");
            entity.material = json_node_string(json_object_member(pair.second, "material"), "");
            entity.texture = json_node_string(json_object_member(pair.second, "texture"), "");
            imported.entities[pair.first] = entity;
        }
    }

    store.currentScene = sceneName;
    store.scenes[sceneName] = imported;
}

std::string save_scene_to_file(const EngineStore& store, const std::string& sceneName, const std::string& requestedPath) {
    const std::filesystem::path path = resolve_scene_path(store, requestedPath, trim(sceneName).empty() ? store.currentScene : trim(sceneName));
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to open scene file for writing: " + path.string());
    }
    out << export_scene_json(store, sceneName);
    return path.string();
}

std::string load_scene_from_file(EngineStore& store, const std::string& requestedPath, const std::string& sceneName) {
    const std::filesystem::path path = resolve_scene_path(store, requestedPath, trim(sceneName).empty() ? store.currentScene : trim(sceneName));
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open scene file for reading: " + path.string());
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    import_scene_json(store, buffer.str(), sceneName);
    return path.string();
}

std::string postfx_json(const EnginePostFX& postfx) {
    std::ostringstream out;
    out << "{";
    out << "\"exposure\":" << postfx.exposure << ",";
    out << "\"vignette\":" << postfx.vignette << ",";
    out << "\"film_grain\":" << postfx.filmGrain << ",";
    out << "\"saturation\":" << postfx.saturation << ",";
    out << "\"contrast\":" << postfx.contrast << ",";
    out << "\"bloom\":" << postfx.bloom << ",";
    out << "\"fog_color\":" << color_json(postfx.fogColor) << ",";
    out << "\"fog_near\":" << postfx.fogNear << ",";
    out << "\"fog_far\":" << postfx.fogFar << ",";
    out << "\"fog_density\":" << postfx.fogDensity << ",";
    out << "\"volumetric\":" << postfx.volumetric;
    out << "}";
    return out.str();
}

std::filesystem::path resolve_watch_path(EngineStore& store, const std::string& requestedPath, const std::string& sceneName) {
    return resolve_scene_path(store, requestedPath, trim(sceneName).empty() ? store.currentScene : trim(sceneName));
}

void tick_scene_hot_reload(EngineStore& store) {
    if (!store.sceneWatch.enabled || store.sceneWatch.path.empty()) {
        return;
    }
    std::error_code ec;
    const auto currentWrite = std::filesystem::last_write_time(store.sceneWatch.path, ec);
    if (ec) {
        return;
    }
    if (store.sceneWatch.lastWrite.time_since_epoch().count() == 0) {
        store.sceneWatch.lastWrite = currentWrite;
        return;
    }
    if (currentWrite != store.sceneWatch.lastWrite) {
        store.sceneWatch.lastWrite = currentWrite;
        load_scene_from_file(store, store.sceneWatch.path.string(), store.sceneWatch.sceneName);
    }
}

int render_scene_entities(EngineStore& store, const std::string& sceneName) {
    EngineScene& scene = ensure_scene(store, sceneName);
    const EngineLight* light = scene.hasSun ? &scene.sun : nullptr;
    rt_set_light_state(
        to_rt_color(scene.ambient),
        light != nullptr ? to_rt_vec3(light->direction) : RTVec3{-0.45f, -1.0f, -0.3f},
        light != nullptr ? to_rt_color(light->color) : RTColor{255, 244, 214, 255},
        light != nullptr ? light->intensity : 0.0f,
        light != nullptr ? 1 : 0);
    rt_set_postfx_state(
        store.postfx.exposure,
        store.postfx.vignette,
        store.postfx.filmGrain,
        store.postfx.saturation,
        store.postfx.contrast,
        store.postfx.bloom,
        to_rt_color(store.postfx.fogColor),
        store.postfx.fogNear,
        store.postfx.fogFar,
        store.postfx.fogDensity,
        store.postfx.volumetric);
    struct RenderItem {
        std::string drawKind;
        std::string textureSource;
        EngineVec3 position;
        EngineVec3 drawSize;
        float drawRadius = 1.0f;
        float planeWidth = 4.0f;
        float planeHeight = 4.0f;
        EngineColor color;
        EngineColor emissive {0, 0, 0, 255};
        float roughness = 0.5f;
        float metallic = 0.0f;
        bool textured = false;
        float depthKey = 0.0f;
    };

    std::vector<RenderItem> queue;
    queue.reserve(scene.entities.size());
    const EngineVec3 cameraForward = vec3_normalize(vec3_sub(store.cameraTarget, store.cameraPosition));

    for (const auto& pair : scene.entities) {
        const EngineEntity& entity = pair.second;
        if (!entity.visible) {
            continue;
        }

        RenderItem item;
        item.drawKind = entity.kind.empty() ? "cube" : entity.kind;
        item.position = entity.position;
        item.drawSize = entity.scale;
        item.drawRadius = std::max(entity.scale.x, std::max(entity.scale.y, entity.scale.z));
        item.planeWidth = std::max(0.1f, entity.scale.x * 4.0f);
        item.planeHeight = std::max(0.1f, entity.scale.z * 4.0f);

        if (!entity.mesh.empty()) {
            const auto meshFound = scene.meshes.find(entity.mesh);
            if (meshFound != scene.meshes.end()) {
                if (!trim(meshFound->second.primitive).empty()) {
                    item.drawKind = trim(meshFound->second.primitive);
                }
                item.drawSize = EngineVec3{
                    meshFound->second.defaultSize.x * std::max(0.05f, entity.scale.x),
                    meshFound->second.defaultSize.y * std::max(0.05f, entity.scale.y),
                    meshFound->second.defaultSize.z * std::max(0.05f, entity.scale.z)
                };
                item.drawRadius = meshFound->second.defaultRadius * std::max(entity.scale.x, std::max(entity.scale.y, entity.scale.z));
                item.planeWidth = std::max(0.1f, meshFound->second.defaultPlaneWidth * entity.scale.x);
                item.planeHeight = std::max(0.1f, meshFound->second.defaultPlaneHeight * entity.scale.z);
            }
        }

        EngineColor color = entity.color;
        EngineMaterial materialState;
        bool hasMaterial = false;
        std::string resolvedTextureSource;
        std::string textureName = trim(entity.texture);
        if (!entity.material.empty()) {
            const auto materialFound = scene.materials.find(entity.material);
            if (materialFound != scene.materials.end()) {
                materialState = materialFound->second;
                hasMaterial = true;
                item.emissive = materialFound->second.emissive;
                item.roughness = materialFound->second.roughness;
                item.metallic = materialFound->second.metallic;
                if (!trim(materialFound->second.texture).empty()) {
                    textureName = trim(materialFound->second.texture);
                }
                color = mix_color(color, materialFound->second.albedo, 0.55f);
                if (!materialFound->second.texture.empty()) {
                    color = mix_color(color, EngineColor{214, 226, 255, 255}, 0.1f);
                }
                color = mix_color(color, EngineColor{255, 255, 255, 255}, std::clamp(materialFound->second.metallic * 0.18f, 0.0f, 0.18f));
                color = mix_color(color, materialFound->second.emissive, 0.22f);
            }
        }

        if (!textureName.empty()) {
            const auto textureFound = scene.textures.find(textureName);
            if (textureFound != scene.textures.end()) {
                resolvedTextureSource = textureFound->second.source;
                item.textured = !trim(resolvedTextureSource).empty();
            }
        }

        item.color = shade_with_light(color, scene.ambient, light);
        item.depthKey = vec3_dot(vec3_sub(item.position, store.cameraPosition), cameraForward);
        if (hasMaterial) {
            item.color = mix_color(item.color, materialState.albedo, 0.35f);
        }
        item.textureSource = resolvedTextureSource;
        queue.push_back(item);
    }

    std::sort(queue.begin(), queue.end(), [](const RenderItem& a, const RenderItem& b) {
        return a.depthKey > b.depthKey;
    });

    std::string activeTextureSource;
    for (const auto& item : queue) {
        if (item.textured && !trim(item.textureSource).empty()) {
            activeTextureSource = trim(item.textureSource);
            break;
        }
    }

    if (!activeTextureSource.empty()) {
        for (auto& texturePair : scene.textures) {
            if (trim(texturePair.second.source) != activeTextureSource) {
                continue;
            }
            if (ensure_texture_rgba_loaded(store, texturePair.second)) {
                rt_upload_texture_rgba(texturePair.second.width, texturePair.second.height, texturePair.second.rgba.data());
            }
            break;
        }
    }

    for (const auto& item : queue) {
        const bool useUploadedTexture = item.textured && !activeTextureSource.empty() && trim(item.textureSource) == activeTextureSource;
        rt_set_material_state(
            to_rt_color(item.color),
            to_rt_color(item.emissive),
            item.roughness,
            item.metallic,
            useUploadedTexture ? 1 : 0);
        rt_set_material_texture_source(useUploadedTexture ? activeTextureSource.c_str() : nullptr);
        if (item.drawKind == "plane") {
            rt_draw_plane(
                to_rt_vec3(item.position),
                RTVec2{item.planeWidth, item.planeHeight},
                to_rt_color(item.color));
        } else if (item.drawKind == "sphere") {
            rt_draw_sphere(
                to_rt_vec3(item.position),
                std::max(0.05f, item.drawRadius),
                to_rt_color(item.color));
        } else {
            rt_draw_cube(
                to_rt_vec3(item.position),
                to_rt_vec3(item.drawSize),
                to_rt_color(item.color));
        }
    }

    rt_set_render_items(static_cast<int>(queue.size()));
    return static_cast<int>(queue.size());
}

std::string render_stats_json(const EngineStore& store) {
    std::ostringstream out;
    out << "{";
    out << "\"backend\":\"" << json_escape(rt_backend_name()) << "\",";
    out << "\"requested_backend\":\"" << json_escape(rt_backend_requested_name()) << "\",";
    out << "\"draw_calls\":" << rt_get_draw_calls() << ",";
    out << "\"render_items\":" << rt_get_render_items() << ",";
    out << "\"target_fps\":" << rt_get_target_fps() << ",";
    out << "\"frame_time\":" << rt_get_frame_time() << ",";
    out << "\"vsync\":" << json_bool(rt_get_vsync() != 0) << ",";
    out << "\"msaa\":" << rt_get_msaa() << ",";
    out << "\"gpu_count\":" << rt_backend_gpu_count() << ",";
    out << "\"supports3d\":" << json_bool(rt_backend_supports_3d() != 0) << ",";
    out << "\"available\":" << json_bool(rt_backend_is_available() != 0) << ",";
    out << "\"placeholder\":" << json_bool(rt_backend_is_placeholder() != 0) << ",";
    out << "\"surface_ready\":" << json_bool(rt_backend_surface_ready() != 0) << ",";
    out << "\"device_ready\":" << json_bool(rt_backend_device_ready() != 0) << ",";
    out << "\"swapchain_ready\":" << json_bool(rt_backend_swapchain_ready() != 0) << ",";
    out << "\"render_pass_ready\":" << json_bool(rt_backend_render_pass_ready() != 0) << ",";
    out << "\"depth_ready\":" << json_bool(rt_backend_depth_ready() != 0) << ",";
    out << "\"texture_sampler_ready\":" << json_bool(rt_backend_texture_sampler_ready() != 0) << ",";
    out << "\"texture_image_ready\":" << json_bool(rt_backend_texture_image_ready() != 0) << ",";
    out << "\"descriptor_set_ready\":" << json_bool(rt_backend_descriptor_set_ready() != 0) << ",";
    out << "\"graphics_pipeline_ready\":" << json_bool(rt_backend_graphics_pipeline_ready() != 0) << ",";
    out << "\"presented_frames\":" << rt_backend_presented_frame_count();
    out << "}";
    return out.str();
}

int execute_with_error(char** outError, const std::function<void()>& work) {
    try {
        clear_error(outError);
        work();
        return 0;
    } catch (const std::exception& error) {
        assign_error(outError, error.what());
        return 1;
    }
}

std::string dispatch_module(EngineStore& store, const std::string& functionName, const std::vector<ScalarValue>& args) {
    if (functionName == "module_info") {
        return "{"
            "\"name\":\"rayquiro.engine\","
            "\"host_api\":\"engine.dll\","
            "\"native_module\":true,"
            "\"scene_owner\":\"engine.dll\","
            "\"editor_owns_scene\":false,"
            "\"rhi\":\"separate\","
            "\"renderer\":\"separate\","
            "\"scene\":\"separate\""
        "}";
    }
    if (functionName == "backend") {
        const std::string name = args.empty() ? "raylib" : to_string(args[0]);
        store.requestedBackend = rt_parse_backend_kind(name);
        rt_set_backend(name.c_str());
        return "\"" + json_escape(rt_backend_name_from_kind(store.requestedBackend)) + "\"";
    }
    if (functionName == "init" || functionName == "window") {
        const int width = args.size() > 0 ? static_cast<int>(to_number(args[0])) : 1280;
        const int height = args.size() > 1 ? static_cast<int>(to_number(args[1])) : 720;
        const std::string title = args.size() > 2 ? to_string(args[2]) : "Raytolfas Engine";
        rt_init(width, height, title.c_str());
        return "true";
    }
    if (functionName == "shutdown") {
        rt_shutdown();
        return "true";
    }
    if (functionName == "should_close") {
        return json_bool(rt_should_close() != 0);
    }
    if (functionName == "begin") {
        tick_scene_hot_reload(store);
        rt_begin();
        return "true";
    }
    if (functionName == "end") {
        rt_end();
        return "true";
    }
    if (functionName == "clear") {
        const EngineColor color = args.empty() ? EngineColor{11, 18, 28, 255} : value_to_color(args[0], EngineColor{11, 18, 28, 255});
        rt_clear(to_rt_color(color));
        return "true";
    }
    if (functionName == "set_camera" || functionName == "camera") {
        const EngineVec3 position = args.size() > 0 ? value_to_vec3(args[0], EngineVec3{6.0f, 6.0f, 6.0f}) : EngineVec3{6.0f, 6.0f, 6.0f};
        const EngineVec3 target = args.size() > 1 ? value_to_vec3(args[1], EngineVec3{0.0f, 0.0f, 0.0f}) : EngineVec3{0.0f, 0.0f, 0.0f};
        const EngineVec3 up = args.size() > 2 ? value_to_vec3(args[2], EngineVec3{0.0f, 1.0f, 0.0f}) : EngineVec3{0.0f, 1.0f, 0.0f};
        const float fov = args.size() > 3 ? static_cast<float>(to_number(args[3])) : 45.0f;
        store.cameraPosition = position;
        store.cameraTarget = target;
        store.cameraUp = up;
        store.cameraFov = fov;
        rt_set_camera(position.x, position.y, position.z, target.x, target.y, target.z, up.x, up.y, up.z, fov);
        return "true";
    }
    if (functionName == "target_fps") {
        const int fps = args.empty() ? 60 : static_cast<int>(to_number(args[0]));
        rt_set_target_fps(fps);
        return json_number(fps);
    }
    if (functionName == "frame_time") {
        return json_number(rt_get_frame_time());
    }
    if (functionName == "camera_fov") {
        if (!args.empty()) {
            store.cameraFov = static_cast<float>(to_number(args[0]));
            rt_set_camera_fov(store.cameraFov);
        }
        return json_number(rt_get_camera_fov());
    }
    if (functionName == "camera_orbit") {
        const float yaw = args.size() > 0 ? static_cast<float>(to_number(args[0])) : 0.0f;
        const float pitch = args.size() > 1 ? static_cast<float>(to_number(args[1])) : 0.0f;
        const float radiusDelta = args.size() > 2 ? static_cast<float>(to_number(args[2])) : 0.0f;
        EngineVec3 offset = vec3_sub(store.cameraPosition, store.cameraTarget);
        float radius = vec3_length(offset);
        radius = std::max(0.001f, radius);
        float yawAngle = std::atan2(offset.z, offset.x);
        float pitchAngle = std::asin(offset.y / radius);
        constexpr float degToRad = 3.14159265358979323846f / 180.0f;
        yawAngle += yaw * degToRad;
        pitchAngle += pitch * degToRad;
        pitchAngle = std::clamp(pitchAngle, -1.4f, 1.4f);
        radius = std::max(0.5f, radius + radiusDelta);
        offset.x = std::cos(pitchAngle) * std::cos(yawAngle) * radius;
        offset.y = std::sin(pitchAngle) * radius;
        offset.z = std::cos(pitchAngle) * std::sin(yawAngle) * radius;
        store.cameraPosition = EngineVec3{
            store.cameraTarget.x + offset.x,
            store.cameraTarget.y + offset.y,
            store.cameraTarget.z + offset.z
        };
        rt_camera_orbit(yaw, pitch, radiusDelta);
        return "true";
    }
    if (functionName == "vsync") {
        if (!args.empty()) {
            rt_set_vsync(to_bool(args[0], true) ? 1 : 0);
        }
        return json_bool(rt_get_vsync() != 0);
    }
    if (functionName == "msaa") {
        if (!args.empty()) {
            rt_set_msaa(static_cast<int>(to_number(args[0])));
        }
        return json_number(rt_get_msaa());
    }
    if (functionName == "exposure") {
        if (!args.empty()) {
            store.postfx.exposure = static_cast<float>(to_number(args[0]));
        }
        return json_number(store.postfx.exposure);
    }
    if (functionName == "vignette") {
        if (!args.empty()) {
            store.postfx.vignette = static_cast<float>(to_number(args[0]));
        }
        return json_number(store.postfx.vignette);
    }
    if (functionName == "film_grain") {
        if (!args.empty()) {
            store.postfx.filmGrain = static_cast<float>(to_number(args[0]));
        }
        return json_number(store.postfx.filmGrain);
    }
    if (functionName == "saturation") {
        if (!args.empty()) {
            store.postfx.saturation = static_cast<float>(to_number(args[0]));
        }
        return json_number(store.postfx.saturation);
    }
    if (functionName == "contrast") {
        if (!args.empty()) {
            store.postfx.contrast = static_cast<float>(to_number(args[0]));
        }
        return json_number(store.postfx.contrast);
    }
    if (functionName == "bloom") {
        if (!args.empty()) {
            store.postfx.bloom = static_cast<float>(to_number(args[0]));
        }
        return json_number(store.postfx.bloom);
    }
    if (functionName == "fog") {
        if (args.size() > 0) {
            store.postfx.fogColor = value_to_color(args[0], store.postfx.fogColor);
        }
        if (args.size() > 1) {
            store.postfx.fogNear = static_cast<float>(to_number(args[1]));
        }
        if (args.size() > 2) {
            store.postfx.fogFar = static_cast<float>(to_number(args[2]));
        }
        if (args.size() > 3) {
            store.postfx.fogDensity = static_cast<float>(to_number(args[3]));
        }
        return postfx_json(store.postfx);
    }
    if (functionName == "volumetric") {
        if (!args.empty()) {
            store.postfx.volumetric = static_cast<float>(to_number(args[0]));
        }
        return json_number(store.postfx.volumetric);
    }
    if (functionName == "postfx_info") {
        return postfx_json(store.postfx);
    }
    if (functionName == "scene_watch") {
        if (args.empty()) {
            store.sceneWatch.enabled = false;
            store.sceneWatch.path.clear();
            store.sceneWatch.sceneName.clear();
            store.sceneWatch.lastWrite = {};
            return "false";
        }
        const std::string sceneName = args.size() > 1 ? to_string(args[1]) : store.currentScene;
        store.sceneWatch.path = resolve_watch_path(store, to_string(args[0]), sceneName);
        store.sceneWatch.sceneName = trim(sceneName).empty() ? store.currentScene : trim(sceneName);
        store.sceneWatch.lastWrite = std::filesystem::last_write_time(store.sceneWatch.path);
        store.sceneWatch.enabled = true;
        return "\"" + json_escape(store.sceneWatch.path.string()) + "\"";
    }
    if (functionName == "scene_reload") {
        const std::string sceneName = args.size() > 1 ? to_string(args[1]) : store.currentScene;
        const std::string pathValue = args.empty()
            ? (store.sceneWatch.enabled ? store.sceneWatch.path.string() : std::string())
            : to_string(args[0]);
        if (trim(pathValue).empty()) {
            return "false";
        }
        return "\"" + json_escape(load_scene_from_file(store, pathValue, sceneName)) + "\"";
    }
    if (functionName == "key_down") {
        return json_bool(!args.empty() && rt_key_down(static_cast<int>(to_number(args[0]))) != 0);
    }
    if (functionName == "key_pressed") {
        return json_bool(!args.empty() && rt_key_pressed(static_cast<int>(to_number(args[0]))) != 0);
    }
    if (functionName == "mouse_down") {
        return json_bool(!args.empty() && rt_mouse_down(static_cast<int>(to_number(args[0]))) != 0);
    }
    if (functionName == "frame_begin") {
        tick_scene_hot_reload(store);
        rt_begin();
        if (!args.empty()) {
            const EngineColor color = value_to_color(args[0], EngineColor{11, 18, 28, 255});
            rt_clear(to_rt_color(color));
        }
        return "true";
    }
    if (functionName == "frame_end") {
        rt_end();
        return "true";
    }
    if (functionName == "backend_name") {
        return "\"" + json_escape(rt_backend_name_from_kind(store.requestedBackend)) + "\"";
    }
    if (functionName == "backend_info") {
        return backend_info_json(store);
    }
    if (functionName == "assets_root") {
        if (!args.empty()) {
            store.assetsRoot = require_name(to_string(args[0]), "assets root");
        }
        return "\"" + json_escape(store.assetsRoot) + "\"";
    }
    if (functionName == "scene") {
        const std::string name = args.empty() ? store.currentScene : to_string(args[0]);
        ensure_scene(store, name);
        return "\"" + json_escape(store.currentScene) + "\"";
    }
    if (functionName == "scene_clear") {
        const std::string name = args.empty() ? store.currentScene : to_string(args[0]);
        store.scenes[require_name(name, "scene")] = EngineScene{};
        store.currentScene = require_name(name, "scene");
        return "true";
    }
    if (functionName == "scene_stats") {
        return scene_stats_json(store, args.empty() ? store.currentScene : to_string(args[0]));
    }
    if (functionName == "stats") {
        std::ostringstream out;
        const auto sceneName = store.currentScene;
        const auto found = store.scenes.find(sceneName);
        const EngineScene emptyScene;
        const EngineScene& scene = (found == store.scenes.end()) ? emptyScene : found->second;
        out << "{";
        out << "\"backend\":\"" << json_escape(rt_backend_name_from_kind(store.requestedBackend)) << "\",";
        out << "\"current_scene\":\"" << json_escape(sceneName) << "\",";
        out << "\"scene_count\":" << store.scenes.size() << ",";
        out << "\"entity_count\":" << scene.entities.size() << ",";
        out << "\"material_count\":" << scene.materials.size() << ",";
        out << "\"mesh_count\":" << scene.meshes.size() << ",";
        out << "\"texture_count\":" << scene.textures.size() << ",";
        out << "\"assets_root\":\"" << json_escape(store.assetsRoot) << "\",";
        out << "\"draw_calls\":" << rt_get_draw_calls() << ",";
        out << "\"render_items\":" << rt_get_render_items() << ",";
        out << "\"frame_time\":" << rt_get_frame_time() << ",";
        out << "\"postfx\":" << postfx_json(store.postfx);
        out << "}";
        return out.str();
    }
    if (functionName == "render_stats") {
        return render_stats_json(store);
    }
    if (functionName == "export_scene") {
        return export_scene_json(store, args.empty() ? store.currentScene : to_string(args[0]));
    }
    if (functionName == "scene_save") {
        const std::string path = args.size() > 0 ? to_string(args[0]) : "";
        const std::string sceneName = args.size() > 1 ? to_string(args[1]) : store.currentScene;
        return "\"" + json_escape(save_scene_to_file(store, sceneName, path)) + "\"";
    }
    if (functionName == "scene_load") {
        const std::string path = args.size() > 0 ? to_string(args[0]) : "";
        const std::string sceneName = args.size() > 1 ? to_string(args[1]) : "";
        return "\"" + json_escape(load_scene_from_file(store, path, sceneName)) + "\"";
    }
    if (functionName == "entity") {
        if (args.size() < 5) throw std::runtime_error("engine.entity requires name, kind, position, size and color.");
        EngineEntity& entity = ensure_entity(store, store.currentScene, to_string(args[0]), args.size() > 1 ? to_string(args[1]) : "");
        entity.position = value_to_vec3(args[2], entity.position);
        entity.scale = EngineVec3{1.0f, 1.0f, 1.0f};
        entity.color = value_to_color(args[4], entity.color);
        if (entity.kind == "sphere") {
            entity.scale = value_to_vec3(args[3], entity.scale);
        } else {
            entity.scale = value_to_vec3(args[3], entity.scale);
        }
        if (args.size() > 5) entity.material = to_string(args[5]);
        if (args.size() > 6) entity.mesh = to_string(args[6]);
        if (args.size() > 7) entity.texture = to_string(args[7]);
        return entity_json(entity);
    }
    if (functionName == "entity_exists") {
        if (args.empty()) return "false";
        const auto sceneFound = store.scenes.find(store.currentScene);
        if (sceneFound == store.scenes.end()) return "false";
        return json_bool(sceneFound->second.entities.find(trim(to_string(args[0]))) != sceneFound->second.entities.end());
    }
    if (functionName == "entity_remove") {
        if (args.empty()) return "false";
        const auto sceneFound = store.scenes.find(store.currentScene);
        if (sceneFound == store.scenes.end()) return "false";
        return json_bool(sceneFound->second.entities.erase(trim(to_string(args[0]))) > 0);
    }
    if (functionName == "entity_get_position") {
        if (args.empty()) return "null";
        const auto sceneFound = store.scenes.find(store.currentScene);
        if (sceneFound == store.scenes.end()) return "null";
        const auto entityFound = sceneFound->second.entities.find(trim(to_string(args[0])));
        if (entityFound == sceneFound->second.entities.end()) return "null";
        std::ostringstream out;
        out << "[" << entityFound->second.position.x << "," << entityFound->second.position.y << "," << entityFound->second.position.z << "]";
        return out.str();
    }
    if (functionName == "entity_set_position") {
        if (args.size() < 2) throw std::runtime_error("engine.entity_set_position requires name and vec3.");
        EngineEntity& entity = ensure_entity(store, store.currentScene, to_string(args[0]), "");
        entity.position = value_to_vec3(args[1], entity.position);
        return entity_json(entity);
    }
    if (functionName == "entity_set_size") {
        if (args.size() < 2) throw std::runtime_error("engine.entity_set_size requires name and size.");
        EngineEntity& entity = ensure_entity(store, store.currentScene, to_string(args[0]), "");
        entity.scale = value_to_vec3(args[1], entity.scale);
        return entity_json(entity);
    }
    if (functionName == "entity_set_scale") {
        if (args.size() < 2) throw std::runtime_error("engine.entity_set_scale requires name and vec3.");
        EngineEntity& entity = ensure_entity(store, store.currentScene, to_string(args[0]), "");
        entity.scale = value_to_vec3(args[1], entity.scale);
        return entity_json(entity);
    }
    if (functionName == "entity_set_radius") {
        if (args.size() < 2) throw std::runtime_error("engine.entity_set_radius requires name and radius.");
        EngineEntity& entity = ensure_entity(store, store.currentScene, to_string(args[0]), "");
        const double radius = to_number(args[1]);
        entity.scale = EngineVec3{static_cast<float>(radius), static_cast<float>(radius), static_cast<float>(radius)};
        return entity_json(entity);
    }
    if (functionName == "entity_set_color") {
        if (args.size() < 2) throw std::runtime_error("engine.entity_set_color requires name and color.");
        EngineEntity& entity = ensure_entity(store, store.currentScene, to_string(args[0]), "");
        entity.color = value_to_color(args[1], entity.color);
        return entity_json(entity);
    }
    if (functionName == "entity_set_visible") {
        if (args.size() < 2) throw std::runtime_error("engine.entity_set_visible requires name and bool.");
        EngineEntity& entity = ensure_entity(store, store.currentScene, to_string(args[0]), "");
        entity.visible = to_bool(args[1], true);
        return entity_json(entity);
    }
    if (functionName == "mesh") {
        if (args.empty()) throw std::runtime_error("engine.mesh requires mesh name.");
        EngineScene& scene = ensure_scene(store, store.currentScene);
        EngineMesh mesh;
        if (args.size() > 1) mesh.primitive = trim(to_string(args[1])).empty() ? "cube" : trim(to_string(args[1]));
        if (args.size() > 2) {
            if (std::holds_alternative<std::string>(args[2].data) && trim(std::get<std::string>(args[2].data)).rfind("[", 0) != 0) {
                mesh.source = to_string(args[2]);
            } else {
                const EngineVec3 size = value_to_vec3(args[2], mesh.defaultSize);
                mesh.defaultSize = size;
                mesh.defaultRadius = std::max(size.x, std::max(size.y, size.z));
                mesh.defaultPlaneWidth = size.x;
                mesh.defaultPlaneHeight = size.z == 0.0f ? size.y : size.z;
            }
        }
        scene.meshes[require_name(to_string(args[0]), "mesh")] = mesh;
        return "true";
    }
    if (functionName == "mesh_exists") {
        if (args.empty()) return "false";
        const auto sceneFound = store.scenes.find(store.currentScene);
        if (sceneFound == store.scenes.end()) return "false";
        return json_bool(sceneFound->second.meshes.find(trim(to_string(args[0]))) != sceneFound->second.meshes.end());
    }
    if (functionName == "texture") {
        if (args.size() < 2) throw std::runtime_error("engine.texture requires texture name and source.");
        EngineScene& scene = ensure_scene(store, store.currentScene);
        EngineTexture texture;
        texture.source = to_string(args[1]);
        texture.srgb = args.size() > 2 ? to_bool(args[2], true) : true;
        texture.normalMap = args.size() > 3 ? to_bool(args[3], false) : false;
        scene.textures[require_name(to_string(args[0]), "texture")] = texture;
        return "true";
    }
    if (functionName == "texture_exists") {
        if (args.empty()) return "false";
        const auto sceneFound = store.scenes.find(store.currentScene);
        if (sceneFound == store.scenes.end()) return "false";
        return json_bool(sceneFound->second.textures.find(trim(to_string(args[0]))) != sceneFound->second.textures.end());
    }
    if (functionName == "material") {
        if (args.empty()) throw std::runtime_error("engine.material requires material name.");
        EngineScene& scene = ensure_scene(store, store.currentScene);
        EngineMaterial material;
        if (args.size() > 1) {
            if (std::holds_alternative<std::string>(args[1].data) && trim(std::get<std::string>(args[1].data)).rfind("[", 0) != 0) {
                material.texture = to_string(args[1]);
            } else {
                material.albedo = value_to_color(args[1], material.albedo);
            }
        }
        if (args.size() > 2) material.roughness = static_cast<float>(to_number(args[2]));
        if (args.size() > 3) material.metallic = static_cast<float>(to_number(args[3]));
        if (args.size() > 4) material.emissive = value_to_color(args[4], material.emissive);
        scene.materials[require_name(to_string(args[0]), "material")] = material;
        return material_json(material);
    }
    if (functionName == "material_exists") {
        if (args.empty()) return "false";
        const auto sceneFound = store.scenes.find(store.currentScene);
        if (sceneFound == store.scenes.end()) return "false";
        return json_bool(sceneFound->second.materials.find(trim(to_string(args[0]))) != sceneFound->second.materials.end());
    }
    if (functionName == "material_texture") {
        if (args.size() < 2) throw std::runtime_error("engine.material_texture requires material and texture.");
        EngineScene& scene = ensure_scene(store, store.currentScene);
        EngineMaterial& material = scene.materials[require_name(to_string(args[0]), "material")];
        material.texture = require_name(to_string(args[1]), "texture");
        return "true";
    }
    if (functionName == "entity_mesh") {
        if (args.size() < 2) throw std::runtime_error("engine.entity_mesh requires entity and mesh.");
        EngineEntity& entity = ensure_entity(store, store.currentScene, to_string(args[0]), "");
        entity.mesh = require_name(to_string(args[1]), "mesh");
        return entity_json(entity);
    }
    if (functionName == "entity_texture") {
        if (args.size() < 2) throw std::runtime_error("engine.entity_texture requires entity and texture.");
        EngineEntity& entity = ensure_entity(store, store.currentScene, to_string(args[0]), "");
        entity.texture = require_name(to_string(args[1]), "texture");
        return entity_json(entity);
    }
    if (functionName == "entity_material") {
        if (args.size() < 2) throw std::runtime_error("engine.entity_material requires entity and material.");
        EngineEntity& entity = ensure_entity(store, store.currentScene, to_string(args[0]), "");
        entity.material = require_name(to_string(args[1]), "material");
        return entity_json(entity);
    }
    if (functionName == "light_ambient") {
        if (!args.empty()) {
            ensure_scene(store, store.currentScene).ambient = value_to_color(args[0], ensure_scene(store, store.currentScene).ambient);
        }
        return "true";
    }
    if (functionName == "light_directional") {
        EngineScene& scene = ensure_scene(store, store.currentScene);
        if (args.size() > 0) scene.sun.direction = value_to_vec3(args[0], scene.sun.direction);
        if (args.size() > 1) scene.sun.color = value_to_color(args[1], scene.sun.color);
        if (args.size() > 2) scene.sun.intensity = static_cast<float>(to_number(args[2]));
        scene.hasSun = true;
        return "true";
    }
    if (functionName == "scene_draw") {
        const int submitted = render_scene_entities(store, args.empty() ? store.currentScene : to_string(args[0]));
        return json_number(submitted);
    }
    if (functionName == "draw_grid") {
        const int slices = args.size() > 0 ? static_cast<int>(to_number(args[0])) : 20;
        const float spacing = args.size() > 1 ? static_cast<float>(to_number(args[1])) : 1.0f;
        rt_draw_grid(slices, spacing);
        return "true";
    }
    if (functionName == "draw_cube") {
        if (args.size() < 3) throw std::runtime_error("__RQIO_FALLBACK__");
        rt_draw_cube(
            to_rt_vec3(value_to_vec3(args[0])),
            to_rt_vec3(value_to_vec3(args[1], EngineVec3{1.0f, 1.0f, 1.0f})),
            to_rt_color(value_to_color(args[2])));
        return "true";
    }
    if (functionName == "draw_plane") {
        if (args.size() < 3) throw std::runtime_error("__RQIO_FALLBACK__");
        const EngineVec3 size3 = value_to_vec3(args[1], EngineVec3{10.0f, 0.0f, 10.0f});
        rt_draw_plane(
            to_rt_vec3(value_to_vec3(args[0])),
            RTVec2{size3.x, size3.z == 0.0f ? size3.y : size3.z},
            to_rt_color(value_to_color(args[2])));
        return "true";
    }
    if (functionName == "draw_sphere") {
        if (args.size() < 3) throw std::runtime_error("__RQIO_FALLBACK__");
        rt_draw_sphere(
            to_rt_vec3(value_to_vec3(args[0])),
            static_cast<float>(to_number(args[1])),
            to_rt_color(value_to_color(args[2])));
        return "true";
    }
    if (functionName == "draw_text") {
        if (args.size() < 5) throw std::runtime_error("__RQIO_FALLBACK__");
        const std::string text = to_string(args[0]);
        rt_draw_text(
            text.c_str(),
            static_cast<int>(to_number(args[1])),
            static_cast<int>(to_number(args[2])),
            static_cast<int>(to_number(args[3])),
            to_rt_color(value_to_color(args[4])));
        return "true";
    }
    if (functionName == "draw_fps") {
        const int x = args.size() > 0 ? static_cast<int>(to_number(args[0])) : 16;
        const int y = args.size() > 1 ? static_cast<int>(to_number(args[1])) : 16;
        rt_draw_fps(x, y);
        return "true";
    }
    if (functionName == "asset_path") {
        if (args.empty()) return "\"" + json_escape(store.assetsRoot) + "\"";
        const std::filesystem::path path = store.projectRoot / store.assetsRoot / to_string(args[0]);
        return "\"" + json_escape(path.lexically_normal().string()) + "\"";
    }
    if (functionName == "asset_exists") {
        if (args.empty()) return "false";
        const std::filesystem::path path = store.projectRoot / store.assetsRoot / to_string(args[0]);
        return json_bool(std::filesystem::exists(path));
    }
    throw std::runtime_error("__RQIO_FALLBACK__");
}
}

struct RQEngineHandle {
    EngineStore store;
};

RQENGINE_EXPORT const char* rqengine_version(void) {
    return "0.0.2-dev";
}

RQENGINE_EXPORT int rqengine_create(const char* project_root, RQEngineHandle** out_handle, char** out_error) {
    if (out_handle == nullptr) {
        assign_error(out_error, "rqengine_create requires an out_handle pointer.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        auto handle = std::make_unique<RQEngineHandle>();
        handle->store.projectRoot = project_root == nullptr ? std::filesystem::current_path() : std::filesystem::absolute(project_root);
        ensure_scene(handle->store, handle->store.currentScene);
        *out_handle = handle.release();
    });
}

RQENGINE_EXPORT void rqengine_destroy(RQEngineHandle* handle) {
    delete handle;
}

RQENGINE_EXPORT int rqengine_set_backend(RQEngineHandle* handle, const char* backend_name, char** out_error) {
    if (handle == nullptr) {
        assign_error(out_error, "rqengine_set_backend requires a valid handle.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        handle->store.requestedBackend = rt_parse_backend_kind((backend_name == nullptr || backend_name[0] == '\0') ? "raylib" : std::string(backend_name));
    });
}

RQENGINE_EXPORT int rqengine_set_assets_root(RQEngineHandle* handle, const char* assets_root, char** out_error) {
    if (handle == nullptr) {
        assign_error(out_error, "rqengine_set_assets_root requires a valid handle.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        handle->store.assetsRoot = require_name(assets_root == nullptr ? "" : std::string(assets_root), "assets root");
    });
}

RQENGINE_EXPORT int rqengine_open_window(RQEngineHandle* handle, int width, int height, const char* title, char** out_error) {
    if (handle == nullptr) {
        assign_error(out_error, "rqengine_open_window requires a valid handle.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        const std::string backendName = rt_backend_name_from_kind(handle->store.requestedBackend);
        rt_set_backend(backendName.c_str());
        rt_init(width <= 0 ? 1280 : width, height <= 0 ? 720 : height, (title == nullptr || title[0] == '\0') ? "Raytolfas Engine" : title);
    });
}

RQENGINE_EXPORT int rqengine_open_embedded_viewport(
    RQEngineHandle* handle,
    void* parent_window,
    int x,
    int y,
    int width,
    int height,
    const char* title,
    char** out_error
) {
    if (handle == nullptr) {
        assign_error(out_error, "rqengine_open_embedded_viewport requires a valid handle.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        const std::string backendName = rt_backend_name_from_kind(handle->store.requestedBackend);
        rt_set_backend(backendName.c_str());
        rt_attach_host_window(parent_window, x, y, width <= 0 ? 640 : width, height <= 0 ? 360 : height, (title == nullptr || title[0] == '\0') ? "Raytolfas Viewport" : title);
        rt_init(width <= 0 ? 640 : width, height <= 0 ? 360 : height, (title == nullptr || title[0] == '\0') ? "Raytolfas Viewport" : title);
    });
}

RQENGINE_EXPORT int rqengine_resize_embedded_viewport(
    RQEngineHandle* handle,
    int x,
    int y,
    int width,
    int height,
    char** out_error
) {
    if (handle == nullptr) {
        assign_error(out_error, "rqengine_resize_embedded_viewport requires a valid handle.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        rt_resize_host_window(x, y, width <= 0 ? 1 : width, height <= 0 ? 1 : height);
    });
}

RQENGINE_EXPORT int rqengine_close_window(RQEngineHandle* handle, char** out_error) {
    if (handle == nullptr) {
        assign_error(out_error, "rqengine_close_window requires a valid handle.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        rt_shutdown();
    });
}

RQENGINE_EXPORT int rqengine_should_close(RQEngineHandle* handle, int* out_should_close, char** out_error) {
    if (handle == nullptr || out_should_close == nullptr) {
        assign_error(out_error, "rqengine_should_close requires a valid handle and out_should_close.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        *out_should_close = rt_should_close();
    });
}

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
    char** out_error
) {
    if (handle == nullptr) {
        assign_error(out_error, "rqengine_set_camera requires a valid handle.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        handle->store.cameraPosition = EngineVec3{px, py, pz};
        handle->store.cameraTarget = EngineVec3{tx, ty, tz};
        handle->store.cameraUp = EngineVec3{ux, uy, uz};
        handle->store.cameraFov = fov;
        rt_set_camera(px, py, pz, tx, ty, tz, ux, uy, uz, fov);
    });
}

RQENGINE_EXPORT int rqengine_begin_frame(RQEngineHandle* handle, int r, int g, int b, int a, char** out_error) {
    if (handle == nullptr) {
        assign_error(out_error, "rqengine_begin_frame requires a valid handle.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        rt_begin();
        rt_clear(RTColor{
            static_cast<unsigned char>(std::clamp(r, 0, 255)),
            static_cast<unsigned char>(std::clamp(g, 0, 255)),
            static_cast<unsigned char>(std::clamp(b, 0, 255)),
            static_cast<unsigned char>(std::clamp(a, 0, 255))
        });
    });
}

RQENGINE_EXPORT int rqengine_end_frame(RQEngineHandle* handle, char** out_error) {
    if (handle == nullptr) {
        assign_error(out_error, "rqengine_end_frame requires a valid handle.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        rt_end();
    });
}

RQENGINE_EXPORT int rqengine_draw_scene(RQEngineHandle* handle, const char* scene_name, int* out_submitted, char** out_error) {
    if (handle == nullptr) {
        assign_error(out_error, "rqengine_draw_scene requires a valid handle.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        const int submitted = render_scene_entities(handle->store, scene_name == nullptr ? handle->store.currentScene : std::string(scene_name));
        if (out_submitted != nullptr) {
            *out_submitted = submitted;
        }
    });
}

RQENGINE_EXPORT int rqengine_render_stats_json(RQEngineHandle* handle, char** out_json, char** out_error) {
    if (handle == nullptr || out_json == nullptr) {
        assign_error(out_error, "rqengine_render_stats_json requires a valid handle and out_json.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        *out_json = duplicate_string(render_stats_json(handle->store));
    });
}

RQENGINE_EXPORT int rqengine_select_scene(RQEngineHandle* handle, const char* scene_name, char** out_error) {
    if (handle == nullptr) {
        assign_error(out_error, "rqengine_select_scene requires a valid handle.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        ensure_scene(handle->store, scene_name == nullptr ? "" : std::string(scene_name));
    });
}

RQENGINE_EXPORT int rqengine_clear_scene(RQEngineHandle* handle, const char* scene_name, char** out_error) {
    if (handle == nullptr) {
        assign_error(out_error, "rqengine_clear_scene requires a valid handle.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        const std::string name = require_name(scene_name == nullptr ? handle->store.currentScene : std::string(scene_name), "scene");
        handle->store.scenes[name] = EngineScene{};
        handle->store.currentScene = name;
    });
}

RQENGINE_EXPORT int rqengine_upsert_entity(
    RQEngineHandle* handle,
    const char* scene_name,
    const char* entity_name,
    const char* kind,
    char** out_error
) {
    if (handle == nullptr) {
        assign_error(out_error, "rqengine_upsert_entity requires a valid handle.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        ensure_entity(
            handle->store,
            scene_name == nullptr ? handle->store.currentScene : std::string(scene_name),
            entity_name == nullptr ? "" : std::string(entity_name),
            kind == nullptr ? "" : std::string(kind));
    });
}

RQENGINE_EXPORT int rqengine_set_entity_position(
    RQEngineHandle* handle,
    const char* scene_name,
    const char* entity_name,
    float x,
    float y,
    float z,
    char** out_error
) {
    if (handle == nullptr) {
        assign_error(out_error, "rqengine_set_entity_position requires a valid handle.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        EngineEntity& entity = ensure_entity(
            handle->store,
            scene_name == nullptr ? handle->store.currentScene : std::string(scene_name),
            entity_name == nullptr ? "" : std::string(entity_name),
            "");
        entity.position = EngineVec3{x, y, z};
    });
}

RQENGINE_EXPORT int rqengine_set_entity_scale(
    RQEngineHandle* handle,
    const char* scene_name,
    const char* entity_name,
    float x,
    float y,
    float z,
    char** out_error
) {
    if (handle == nullptr) {
        assign_error(out_error, "rqengine_set_entity_scale requires a valid handle.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        EngineEntity& entity = ensure_entity(
            handle->store,
            scene_name == nullptr ? handle->store.currentScene : std::string(scene_name),
            entity_name == nullptr ? "" : std::string(entity_name),
            "");
        entity.scale = EngineVec3{x, y, z};
    });
}

RQENGINE_EXPORT int rqengine_set_entity_color(
    RQEngineHandle* handle,
    const char* scene_name,
    const char* entity_name,
    int r,
    int g,
    int b,
    int a,
    char** out_error
) {
    if (handle == nullptr) {
        assign_error(out_error, "rqengine_set_entity_color requires a valid handle.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        EngineEntity& entity = ensure_entity(
            handle->store,
            scene_name == nullptr ? handle->store.currentScene : std::string(scene_name),
            entity_name == nullptr ? "" : std::string(entity_name),
            "");
        entity.color = EngineColor{r, g, b, a};
    });
}

RQENGINE_EXPORT int rqengine_bind_entity_resources(
    RQEngineHandle* handle,
    const char* scene_name,
    const char* entity_name,
    const char* mesh_name,
    const char* material_name,
    const char* texture_name,
    char** out_error
) {
    if (handle == nullptr) {
        assign_error(out_error, "rqengine_bind_entity_resources requires a valid handle.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        EngineEntity& entity = ensure_entity(
            handle->store,
            scene_name == nullptr ? handle->store.currentScene : std::string(scene_name),
            entity_name == nullptr ? "" : std::string(entity_name),
            "");
        entity.mesh = mesh_name == nullptr ? std::string() : trim(mesh_name);
        entity.material = material_name == nullptr ? std::string() : trim(material_name);
        entity.texture = texture_name == nullptr ? std::string() : trim(texture_name);
    });
}

RQENGINE_EXPORT int rqengine_upsert_mesh(
    RQEngineHandle* handle,
    const char* scene_name,
    const char* mesh_name,
    const char* primitive,
    float sx,
    float sy,
    float sz,
    const char* source,
    char** out_error
) {
    if (handle == nullptr) {
        assign_error(out_error, "rqengine_upsert_mesh requires a valid handle.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        EngineScene& scene = ensure_scene(handle->store, scene_name == nullptr ? handle->store.currentScene : std::string(scene_name));
        EngineMesh mesh;
        mesh.primitive = primitive == nullptr ? "cube" : require_name(primitive, "primitive");
        mesh.defaultSize = EngineVec3{sx, sy, sz};
        mesh.defaultRadius = std::max(sx, std::max(sy, sz));
        mesh.defaultPlaneWidth = sx;
        mesh.defaultPlaneHeight = sz == 0.0f ? sy : sz;
        mesh.source = source == nullptr ? std::string() : trim(source);
        scene.meshes[require_name(mesh_name == nullptr ? "" : std::string(mesh_name), "mesh")] = mesh;
    });
}

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
    char** out_error
) {
    if (handle == nullptr) {
        assign_error(out_error, "rqengine_upsert_material requires a valid handle.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        EngineScene& scene = ensure_scene(handle->store, scene_name == nullptr ? handle->store.currentScene : std::string(scene_name));
        EngineMaterial material;
        material.albedo = EngineColor{r, g, b, a};
        material.roughness = roughness;
        material.metallic = metallic;
        material.emissive = EngineColor{er, eg, eb, ea};
        material.texture = texture_name == nullptr ? std::string() : trim(texture_name);
        scene.materials[require_name(material_name == nullptr ? "" : std::string(material_name), "material")] = material;
    });
}

RQENGINE_EXPORT int rqengine_upsert_texture(
    RQEngineHandle* handle,
    const char* scene_name,
    const char* texture_name,
    const char* source,
    int srgb,
    int normal_map,
    char** out_error
) {
    if (handle == nullptr) {
        assign_error(out_error, "rqengine_upsert_texture requires a valid handle.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        EngineScene& scene = ensure_scene(handle->store, scene_name == nullptr ? handle->store.currentScene : std::string(scene_name));
        EngineTexture texture;
        texture.source = source == nullptr ? std::string() : require_name(source, "texture source");
        texture.srgb = srgb != 0;
        texture.normalMap = normal_map != 0;
        scene.textures[require_name(texture_name == nullptr ? "" : std::string(texture_name), "texture")] = texture;
    });
}

RQENGINE_EXPORT int rqengine_set_ambient_light(
    RQEngineHandle* handle,
    const char* scene_name,
    int r,
    int g,
    int b,
    int a,
    char** out_error
) {
    if (handle == nullptr) {
        assign_error(out_error, "rqengine_set_ambient_light requires a valid handle.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        EngineScene& scene = ensure_scene(handle->store, scene_name == nullptr ? handle->store.currentScene : std::string(scene_name));
        scene.ambient = EngineColor{r, g, b, a};
    });
}

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
    char** out_error
) {
    if (handle == nullptr) {
        assign_error(out_error, "rqengine_set_directional_light requires a valid handle.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        EngineScene& scene = ensure_scene(handle->store, scene_name == nullptr ? handle->store.currentScene : std::string(scene_name));
        scene.sun.direction = EngineVec3{dx, dy, dz};
        scene.sun.color = EngineColor{r, g, b, a};
        scene.sun.intensity = intensity;
        scene.hasSun = true;
    });
}

RQENGINE_EXPORT int rqengine_backend_info_json(RQEngineHandle* handle, char** out_json, char** out_error) {
    if (handle == nullptr || out_json == nullptr) {
        assign_error(out_error, "rqengine_backend_info_json requires a valid handle and out_json.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        *out_json = duplicate_string(backend_info_json(handle->store));
    });
}

RQENGINE_EXPORT int rqengine_scene_stats_json(RQEngineHandle* handle, const char* scene_name, char** out_json, char** out_error) {
    if (handle == nullptr || out_json == nullptr) {
        assign_error(out_error, "rqengine_scene_stats_json requires a valid handle and out_json.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        *out_json = duplicate_string(scene_stats_json(handle->store, scene_name == nullptr ? handle->store.currentScene : std::string(scene_name)));
    });
}

RQENGINE_EXPORT int rqengine_export_scene_json(RQEngineHandle* handle, const char* scene_name, char** out_json, char** out_error) {
    if (handle == nullptr || out_json == nullptr) {
        assign_error(out_error, "rqengine_export_scene_json requires a valid handle and out_json.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        *out_json = duplicate_string(export_scene_json(handle->store, scene_name == nullptr ? handle->store.currentScene : std::string(scene_name)));
    });
}

RQENGINE_EXPORT int rqengine_save_scene(RQEngineHandle* handle, const char* scene_name, const char* file_path, char** out_path, char** out_error) {
    if (handle == nullptr || out_path == nullptr) {
        assign_error(out_error, "rqengine_save_scene requires a valid handle and out_path.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        const std::string resolvedScene = scene_name == nullptr ? handle->store.currentScene : std::string(scene_name);
        const std::string path = file_path == nullptr ? std::string() : std::string(file_path);
        *out_path = duplicate_string(save_scene_to_file(handle->store, resolvedScene, path));
    });
}

RQENGINE_EXPORT int rqengine_load_scene(RQEngineHandle* handle, const char* scene_name, const char* file_path, char** out_path, char** out_error) {
    if (handle == nullptr || out_path == nullptr) {
        assign_error(out_error, "rqengine_load_scene requires a valid handle and out_path.");
        return 1;
    }
    return execute_with_error(out_error, [&]() {
        const std::string resolvedScene = scene_name == nullptr ? std::string() : std::string(scene_name);
        const std::string path = file_path == nullptr ? std::string() : std::string(file_path);
        *out_path = duplicate_string(load_scene_from_file(handle->store, path, resolvedScene));
        rt_set_camera(
            handle->store.cameraPosition.x,
            handle->store.cameraPosition.y,
            handle->store.cameraPosition.z,
            handle->store.cameraTarget.x,
            handle->store.cameraTarget.y,
            handle->store.cameraTarget.z,
            handle->store.cameraUp.x,
            handle->store.cameraUp.y,
            handle->store.cameraUp.z,
            handle->store.cameraFov);
    });
}

RQENGINE_EXPORT void rqengine_free_string(char* value) {
    delete[] value;
}

RQM_EXPORT int rqm_invoke(const char* function_name, const char* json_args, char** json_result, char** error_message) {
    try {
        std::vector<ScalarValue> args = parse_args(json_args);
        const std::string result = dispatch_module(module_store(), function_name == nullptr ? "" : std::string(function_name), args);
        if (json_result != nullptr) {
            *json_result = duplicate_string(result);
        }
        if (error_message != nullptr) {
            *error_message = nullptr;
        }
        return 0;
    } catch (const std::exception& error) {
        if (json_result != nullptr) {
            *json_result = duplicate_string("null");
        }
        if (error_message != nullptr) {
            *error_message = duplicate_string(error.what());
        }
        return 1;
    }
}

RQM_EXPORT void rqm_free(char* memory) {
    delete[] memory;
}
