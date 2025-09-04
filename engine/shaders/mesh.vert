#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(push_constant) uniform PushConstants {
    mat4 vp;
    vec4 pc0;
    vec4 lightDir;
    vec4 lightColor;
} pc;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 outLightDir;
layout(location = 3) out vec4 outCurrClip;
layout(location = 4) out vec4 outPrevClip;

layout(set = 0, binding = 0) uniform UBO {
    mat4 model;
    mat4 prevModel;
    mat4 prevVP;
} ubo;

void main() {
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);
    gl_Position = pc.vp * worldPos;
    outCurrClip = gl_Position;
    outPrevClip = ubo.prevVP * (ubo.prevModel * vec4(inPosition, 1.0));
    outNormal = mat3(ubo.model) * inNormal;
    outTexCoord = inTexCoord;
    outLightDir = pc.lightDir.xyz;
}
