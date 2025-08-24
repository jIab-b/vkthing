#version 450
layout(location=0) in vec3 inPos;
layout(location=1) in vec3 inNormal;
layout(location=2) in float inHeight;

layout(push_constant) uniform Push {
    mat4 vp;           // 64 bytes
    vec4 pc0;          // x = pointSize
    vec4 lightDir;     // xyz = dir
    vec4 lightColor;   // xyz = color, w = intensity
} pushC;

layout(location=0) out float vHeight;
layout(location=1) out vec3 vNormal;

void main(){
    vHeight = inHeight;
    vNormal = normalize(inNormal);
    gl_Position = pushC.vp * vec4(inPos, 1.0);
    float dist = length(gl_Position.xyz / gl_Position.w);
    gl_PointSize = clamp(pushC.pc0.x / max(dist, 0.001), 1.0, 8.0);
}
