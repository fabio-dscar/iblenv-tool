#include <common.frag>

in vec2 FsTexCoords;
out vec2 FragColor;

layout(location = 1) uniform int NumSamples;

vec2 IntegrateBRDF(float NdotV, float rough) {
    vec3 V = vec3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);

    float I1 = 0.0;
    float I2 = 0.0;

    vec3 N = vec3(0.0, 0.0, 1.0);
    for (int i = 0; i < NumSamples; ++i) {
        vec2 Xi = Hammersley(i, NumSamples);
        vec3 H = SampleGGX(Xi, N, rough);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = clamp(L.z, 0.0, 1.0);
        float NdotH = clamp(H.z, 0.0, 1.0);
        float VdotH = clamp(dot(V, H), 0.0, 1.0);

        if (NdotL > 0.0) {
            float G = GeoSmith(NdotV, NdotL, rough);
            float InvPdf = 4 * VdotH / NdotH;
            float GVis = G * InvPdf * NdotL;
            float Fc = pow(1.0 - VdotH, 5.0);

#ifdef MULTISCATTERING
            I1 += Fc * GVis;
            I2 += GVis;
#else
            I1 += (1.0 - Fc) * GVis;
            I2 += Fc * GVis;
#endif
        }
    }

    I1 /= float(NumSamples);
    I2 /= float(NumSamples);

    return vec2(I1, I2);
}

void main() {
#ifdef FLIP_V
    FragColor = IntegrateBRDF(FsTexCoords.x, 1 - FsTexCoords.y);
#else
    FragColor = IntegrateBRDF(FsTexCoords.x, FsTexCoords.y);
#endif
}