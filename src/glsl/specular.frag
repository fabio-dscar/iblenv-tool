#include <common.frag>

in vec3 WorldPos;
out vec4 FragColor;

layout(location = 3) uniform samplerCube EnvMap;
layout(location = 4) uniform int NumSamples;
layout(location = 5) uniform float Roughness;

vec3 EnvSpecularConvolution(vec3 N) {
    vec3 V = N; // Simplification

    vec3 Lsum = vec3(0.0);
    float Weight = 0.0;

#ifdef PREFILTERED_IS
    // Pre-filtered importance sampling constants
    ivec2 CubeSize = textureSize(EnvMap, 0);
    float CubeSize2 = CubeSize.x * CubeSize.x;
    float Omega_p = 4.0 * PI / (6.0 * CubeSize2);
    float K = 4.0;
#endif

    for (int i = 0; i < NumSamples; ++i) {
        vec2 Xi = Hammersley(i, NumSamples);
        vec3 H = SampleGGX(Xi, N, Roughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = clamp(dot(N, L), 0.0, 1.0);
        float NdotV = 1.0;  // N = V assumption

        if (NdotL > 0.0) {
            float G = GeoSmith(NdotV, NdotL, Roughness);

#ifdef PREFILTERED_IS
            float NdotH = clamp(dot(N, H), 0.0, 1.0);
            float VdotH = clamp(dot(H, V), 0.0, 1.0);

            // Pre-filtered importance sampling
            float Pdf = DistGGX(N, H, Roughness) * NdotH / (4.0 * VdotH);
            float Omega_s = 1.0 / (float(NumSamples) * Pdf);
            float MipLevel = Roughness == 0.0 ? 0.0 : 0.5 * log2(K * Omega_s / Omega_p);
#else
            float MipLevel = 0.0;
#endif
            vec3 Lenv = textureLod(EnvMap, L, MipLevel).rgb;
            Lsum += Lenv * G * NdotL;
            Weight += NdotL;
        }
    }

    return Lsum / Weight;
}

void main() {
    vec3 Normal = normalize(WorldPos);

    FragColor = vec4(EnvSpecularConvolution(Normal), 1.0);
}