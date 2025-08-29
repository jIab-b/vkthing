#version 450

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inLightDir;

layout(push_constant) uniform PushConstants {
    mat4 vp;
    vec4 pc0;
    vec4 lightDir;
    vec4 lightColor;
} pc;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 normal = normalize(inNormal);
    vec3 lightDir = normalize(inLightDir);

    float diffuse = max(dot(normal, -lightDir), 0.1);
    vec3 color = pc.lightColor.rgb * pc.lightColor.a * diffuse;

    // Simple checkerboard pattern for visualization
    vec2 uv = inTexCoord * 8.0;
    float checker = mod(floor(uv.x) + floor(uv.y), 2.0);
    color *= mix(vec3(0.8), vec3(1.2), checker);

    outColor = vec4(color, 1.0);
}
