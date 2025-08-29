#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(push_constant) uniform PushConstants {
    mat4 vp;
    vec4 pc0; // unused for meshes
    vec4 lightDir;
    vec4 lightColor;
} pc;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 outLightDir;

void main() {
    gl_Position = pc.vp * vec4(inPosition, 1.0);
    outNormal = inNormal;
    outTexCoord = inTexCoord;
    outLightDir = pc.lightDir.xyz;
}
