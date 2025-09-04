#version 450

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inLightDir;
layout(location = 3) in vec4 inCurrClip;
layout(location = 4) in vec4 inPrevClip;

layout(push_constant) uniform PushConstants {
    mat4 vp;
    vec4 pc0;
    vec4 lightDir;
    vec4 lightColor;
} pc;

layout(location = 0) out vec4 outSwapColor;
layout(location = 1) out vec4 outGColor;
layout(location = 2) out vec4 outGNormal;
layout(location = 3) out vec2 outGMotion;

void main() {
    vec3 n = normalize(inNormal);
    vec3 l = normalize(inLightDir);
    float diff = max(dot(n, -l), 0.1);
    vec3 color = pc.lightColor.rgb * pc.lightColor.a * diff;

    outSwapColor = vec4(color, 1.0);
    outGColor = vec4(color, 1.0);
    outGNormal = vec4(normalize(n) * 0.5 + 0.5, 1.0);

    vec2 currNDC = inCurrClip.xy / max(inCurrClip.w, 1e-5);
    vec2 prevNDC = inPrevClip.xy / max(inPrevClip.w, 1e-5);
    outGMotion = currNDC - prevNDC;
}
