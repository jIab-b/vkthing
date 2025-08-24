#version 450
layout(location=0) in float vHeight;
layout(location=1) in vec3 vNormal;
layout(location=0) out vec4 outColor;

layout(push_constant) uniform Push {
    mat4 vp;
    vec4 pc0;        // x=pointSize
    vec4 lightDir;   // xyz
    vec4 lightColor; // xyz color, w intensity
} pushC;

void main(){
    // circular sprite
    vec2 pc = gl_PointCoord * 2.0 - 1.0;
    if (dot(pc, pc) > 1.0) discard;
    // height-based base color
    float h = clamp(vHeight / 60.0, 0.0, 1.0);
    vec3 low = vec3(0.1, 0.4, 0.2);
    vec3 high = vec3(0.9, 0.9, 0.9);
    vec3 col = mix(low, high, pow(h, 1.5));
    // simple directional diffuse + ambient
    vec3 L = normalize(-pushC.lightDir.xyz);
    vec3 N = normalize(vNormal);
    float ndl = max(dot(N, L), 0.0);
    vec3 ambient = 0.25 * col;
    vec3 diffuse = ndl * col * pushC.lightColor.xyz * pushC.lightColor.w;
    outColor = vec4(ambient + diffuse, 1.0);
}
