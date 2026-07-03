#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in float fragDepth;
layout(location = 3) in vec2 fragUv;
layout(location = 4) in vec3 fragEmissive;
layout(location = 5) in vec3 fragMaterial;
layout(location = 6) in vec3 fragWorldPos;
layout(location = 0) out vec4 outColor;
layout(set = 0, binding = 0) uniform sampler2D sceneTexture;
layout(push_constant) uniform ScenePushConstants {
    mat4 mvp;
    vec4 viewportPostfx;
    vec4 fogParams;
    vec4 fogColor;
    vec4 grading;
} pc;

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec3 applySaturation(vec3 color, float saturation) {
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    return mix(vec3(luma), color, saturation);
}

void main() {
    float roughness = clamp(fragMaterial.x, 0.04, 1.0);
    float metallic = clamp(fragMaterial.y, 0.0, 1.0);
    float textured = fragMaterial.z;

    vec3 base = fragColor;
    if (textured > 0.5) {
        vec3 sampled = texture(sceneTexture, fragUv).rgb;
        base *= mix(vec3(0.75), sampled * 1.18, 0.88);
    }

    vec3 lit = base + fragEmissive * (0.65 + pc.grading.z * 1.6) + vec3((1.0 - roughness) * 0.03 + metallic * 0.06);
    lit *= pc.viewportPostfx.z;
    lit = applySaturation(lit, pc.grading.x);
    lit = (lit - 0.5) * pc.grading.y + 0.5;
    lit = lit / (lit + vec3(1.0));

    vec2 screenUv = gl_FragCoord.xy / max(pc.viewportPostfx.xy, vec2(1.0));
    vec2 vignetteUv = screenUv * (1.0 - screenUv.yx);
    float vignette = pow(vignetteUv.x * vignetteUv.y * 15.0, 0.25);
    lit *= mix(1.0 - pc.viewportPostfx.w, 1.0, vignette);

    float fogFactor = smoothstep(pc.fogParams.x, pc.fogParams.y, fragDepth * pc.fogParams.y);
    fogFactor = clamp(fogFactor * max(0.001, pc.fogParams.z), 0.0, 1.0);
    vec3 fogColor = pc.fogColor.rgb;
    float volumetric = clamp(pc.fogParams.w * (1.0 - fragDepth) * 0.35, 0.0, 1.0);
    lit = mix(lit, fogColor, fogFactor * 0.65) + fogColor * volumetric * 0.18;

    float grain = (hash12(gl_FragCoord.xy) - 0.5) * pc.fogColor.a;
    lit += grain;
    outColor = vec4(clamp(lit, 0.0, 1.0), 1.0);
}
