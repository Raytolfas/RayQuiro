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

vec3 acesTonemap(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    float roughness = clamp(fragMaterial.x, 0.04, 1.0);
    float metallic = clamp(fragMaterial.y, 0.0, 1.0);
    float textured = fragMaterial.z;

    vec3 albedo = fragColor;
    if (textured > 0.5) {
        vec3 sampled = texture(sceneTexture, fragUv).rgb;
        albedo *= sampled;
    }

    vec3 N = normalize(fragNormal);
    vec3 sunDir = normalize(vec3(0.4, 0.8, 0.3));
    vec3 viewDir = normalize(vec3(0.0, 2.0, 5.0) - fragWorldPos);
    vec3 halfDir = normalize(sunDir + viewDir);

    float NdotL = max(dot(N, sunDir), 0.0);
    float NdotH = max(dot(N, halfDir), 0.0);

    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom = (NdotH * NdotH * (alpha2 - 1.0) + 1.0);
    float D = alpha2 / max(3.14159265 * denom * denom, 0.0001);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = F0 + (1.0 - F0) * pow(clamp(1.0 - max(dot(halfDir, viewDir), 0.0), 0.0, 1.0), 5.0);

    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float G1 = NdotL / (NdotL * (1.0 - k) + k);
    float NdotV = max(dot(N, viewDir), 0.001);
    float G2 = NdotV / (NdotV * (1.0 - k) + k);
    float G = G1 * G2;

    vec3 specular = (D * F * G) / max(4.0 * NdotL * NdotV, 0.001);
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / 3.14159265;

    vec3 ambient = vec3(0.05, 0.07, 0.1) * albedo;
    vec3 sunLight = (diffuse + specular) * vec3(1.4, 1.35, 1.2) * NdotL;

    vec3 pointLightPos = vec3(0.0, 3.0, 0.0);
    vec3 toPoint = pointLightPos - fragWorldPos;
    float dist = length(toPoint);
    vec3 pointDir = normalize(toPoint);
    float pointNdotL = max(dot(N, pointDir), 0.0);
    float atten = 1.0 / max(1.0 + 0.1 * dist + 0.02 * dist * dist, 0.01);
    vec3 pointLight = albedo * vec3(0.8, 0.9, 1.1) * pointNdotL * atten;

    vec3 lit = ambient + sunLight + pointLight + fragEmissive * (1.0 + pc.grading.z * 2.0);

    lit *= pc.viewportPostfx.z;
    lit = applySaturation(lit, pc.grading.x);
    lit = (lit - 0.5) * pc.grading.y + 0.5;
    lit = acesTonemap(lit);

    vec2 screenUv = gl_FragCoord.xy / max(pc.viewportPostfx.xy, vec2(1.0));
    vec2 vignetteUv = screenUv * (1.0 - screenUv.yx);
    float vignette = pow(max(0.0, vignetteUv.x * vignetteUv.y * 15.0), 0.25);
    lit *= mix(1.0 - pc.viewportPostfx.w, 1.0, vignette);

    float fogFactor = smoothstep(pc.fogParams.x, pc.fogParams.y, fragDepth * pc.fogParams.y);
    fogFactor = clamp(fogFactor * max(0.001, pc.fogParams.z), 0.0, 1.0);
    vec3 fogColor = pc.fogColor.rgb;
    float volumetric = clamp(pc.fogParams.w * (1.0 - fragDepth) * 0.35, 0.0, 1.0);
    lit = mix(lit, fogColor, fogFactor * 0.75) + fogColor * volumetric * 0.15;

    float grain = (hash12(gl_FragCoord.xy) - 0.5) * pc.fogColor.a;
    lit += grain;

    outColor = vec4(clamp(lit, 0.0, 1.0), 1.0);
}
