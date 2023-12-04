#include <common.frag>

out vec4 fragColor;
in vec3 worldPos;

layout(location = 1) uniform uint numSamples;
layout(location = 2) uniform samplerCube envMap;

vec3 SampleHemisphere(vec2 Xi, vec3 N) {
    float phi = 2 * PI * Xi.x;
    float cosTheta = Xi.y;
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    // From tangent-space to world-space
    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

void main() {
    vec3 N = normalize(worldPos);

    vec3 irradiance = vec3(0.0);

    ivec2 cubeSize = textureSize(envMap, 0);
    float pdf = 2 * PI / (6 * cubeSize.x * cubeSize.x);

    for (uint i = 0u; i < numSamples; ++i) {
        vec2 Xi = Hammersley(i, numSamples);
        vec3 S = SampleHemisphere(Xi, N);
        // float pdf = 1.0 / (2.0 * PI);

        float NdotS = clamp(dot(N, S), 0.0, 1.0);
        if (NdotS > 0)
            irradiance += texture(envMap, sampleVec).rgb * NdotS * pdf;
    }

    fragColor = vec4(irradiance, 1.0);
}