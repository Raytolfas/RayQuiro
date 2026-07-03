#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inUv;
layout(location = 4) in vec3 inEmissive;
layout(location = 5) in vec3 inMaterial;

layout(push_constant) uniform ScenePushConstants {
    mat4 mvp;
    vec4 viewportPostfx;
    vec4 fogParams;
    vec4 fogColor;
    vec4 grading;
} pc;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out float fragDepth;
layout(location = 3) out vec2 fragUv;
layout(location = 4) out vec3 fragEmissive;
layout(location = 5) out vec3 fragMaterial;
layout(location = 6) out vec3 fragWorldPos;

void main() {
    vec4 clip = pc.mvp * vec4(inPosition, 1.0);
    gl_Position = clip;
    fragColor = inColor;
    fragNormal = normalize(inNormal);
    fragDepth = clamp(clip.z / max(clip.w, 0.0001), 0.0, 1.0);
    fragUv = inUv;
    fragEmissive = inEmissive;
    fragMaterial = inMaterial;
    fragWorldPos = inPosition;
}
