#pragma once

#include <string>
#include <vector>

enum class RTBackendKind {
    Raylib,
    Vulkan
};

struct RTBackendCapabilities {
    std::string name = "raylib";
    bool supports3D = true;
    bool available = true;
    bool placeholder = false;
    bool isVulkanFamily = false;
    bool supportsPbr = true;
    bool supportsMultiLights = true;
    bool supportsRaycasting = true;
    bool supportsPostFX = true;
    int maxLights = 8;
};

struct RTMaterialState {
    unsigned char albedoR = 255;
    unsigned char albedoG = 255;
    unsigned char albedoB = 255;
    unsigned char albedoA = 255;
    unsigned char emissiveR = 0;
    unsigned char emissiveG = 0;
    unsigned char emissiveB = 0;
    unsigned char emissiveA = 255;
    float roughness = 0.5f;
    float metallic = 0.0f;
    float occlusion = 1.0f;
    bool textured = false;
    std::string textureSource;
    std::string normalMapSource;
};

struct RTPointLightState {
    float posX = 0.0f;
    float posY = 3.0f;
    float posZ = 0.0f;
    unsigned char colorR = 255;
    unsigned char colorG = 255;
    unsigned char colorB = 255;
    float intensity = 1.0f;
    float radius = 15.0f;
    bool enabled = true;
};

struct RTLightState {
    unsigned char ambientR = 30;
    unsigned char ambientG = 38;
    unsigned char ambientB = 54;
    unsigned char ambientA = 255;
    float directionX = -0.45f;
    float directionY = -1.0f;
    float directionZ = -0.3f;
    unsigned char colorR = 255;
    unsigned char colorG = 244;
    unsigned char colorB = 214;
    unsigned char colorA = 255;
    float intensity = 1.2f;
    bool enabled = true;
    std::vector<RTPointLightState> pointLights;
};

struct RTSkyState {
    unsigned char topR = 24;
    unsigned char topG = 38;
    unsigned char topB = 68;
    unsigned char bottomR = 8;
    unsigned char bottomG = 12;
    unsigned char bottomB = 22;
    float sunSize = 0.04f;
    bool enabled = true;
};

struct RTPostFXState {
    float exposure = 1.0f;
    float vignette = 0.08f;
    float filmGrain = 0.02f;
    float saturation = 1.0f;
    float contrast = 1.05f;
    float bloom = 0.08f;
    float bloomThreshold = 0.85f;
    float chromaticAberration = 0.0f;
    float fogNear = 10.0f;
    float fogFar = 48.0f;
    float fogDensity = 0.12f;
    float volumetric = 0.18f;
    unsigned char fogR = 16;
    unsigned char fogG = 20;
    unsigned char fogB = 32;
    unsigned char fogA = 255;
};

struct RTBackendState {
    RTBackendKind requestedKind = RTBackendKind::Raylib;
    RTBackendKind activeKind = RTBackendKind::Raylib;
    bool windowReady = false;
    bool drawingReady = false;
    bool mode3dReady = false;
    bool vsyncEnabled = false;
    int msaaSamples = 4;
    int targetFps = 60;
    int frameDrawCalls = 0;
    int frameRenderItems = 0;
    RTMaterialState materialState = {};
    RTLightState lightState = {};
    RTSkyState skyState = {};
    RTPostFXState postfxState = {};
    void* hostParentWindow = nullptr;
    int hostX = 0;
    int hostY = 0;
    int hostW = 0;
    int hostH = 0;
};

inline RTBackendKind rt_parse_backend_kind(const std::string& name) {
    if (name == "vulkan") {
        return RTBackendKind::Vulkan;
    }
    return RTBackendKind::Raylib;
}

inline std::string rt_backend_name_from_kind(RTBackendKind kind) {
    switch (kind) {
    case RTBackendKind::Vulkan:
        return "vulkan";
    case RTBackendKind::Raylib:
    default:
        return "raylib";
    }
}

inline RTBackendCapabilities rt_backend_capabilities(RTBackendKind kind) {
    switch (kind) {
    case RTBackendKind::Vulkan:
        return RTBackendCapabilities{"vulkan", true, true, false, true, true, true, true, true, 8};
    case RTBackendKind::Raylib:
    default:
        return RTBackendCapabilities{"raylib", true, true, false, false, true, true, true, true, 8};
    }
}
