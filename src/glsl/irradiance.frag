#include <common.frag>

out vec4 FragColor;
in vec3 WorldPos;

layout(location = 3) uniform samplerCube EnvMap;
layout(location = 4) uniform int NumSamples;

const float CosHemisPdf = 1.0 / PI;  // CosTheta cancels with the one in the sum

vec3 EnvironmentIrradiance(vec3 N) {
    vec3 E_d = vec3(0.0);

#ifdef PREFILTERED_IS
    // Pre-filtered importance sampling constants
    ivec2 cubeSize = textureSize(EnvMap, 0);
    float cubeSize2 = cubeSize.x * cubeSize.x;
    float Omega_p = 4.0 * PI / (6.0 * cubeSize2);
    float K = 4.0;
#endif

    for (int i = 0; i < NumSamples; ++i) {
        vec2 Xi = Hammersley(i, NumSamples);
        vec3 Wi = SampleCosWeightedHemis(Xi, N);

        float NdotWi = clamp(dot(N, Wi), 0.0, 1.0);

        if (NdotWi > 0) {
#ifdef PREFILTERED_IS
            float Pdf = NdotWi / PI; // Use pdf of IS we're performing
            float Omega_s = 1.0 / (float(NumSamples) * Pdf);
            float MipLevel = 0.5 * log2(K * Omega_s / Omega_p);
#else
            float MipLevel = 0;
#endif
            vec3 Lenv = textureLod(EnvMap, Wi, MipLevel).rgb;
            E_d += Lenv / CosHemisPdf;
        }
    }

#ifdef DIVIDED_PI
    return E_d / (PI * NumSamples);
#else
    return E_d / NumSamples;
#endif
}

void main() {
    vec3 Normal = normalize(WorldPos);

    FragColor = vec4(EnvironmentIrradiance(Normal), 1.0);
}