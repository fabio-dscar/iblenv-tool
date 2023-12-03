in vec2 fsTexCoords;
out vec2 FragColor;

layout(location = 1) uniform uint numSamples;

// Forward declarations
float DistGGX(vec3 N, vec3 H, float roughness);
vec3 SampleGGX(vec2 Xi, vec3 N, float roughness);
vec2 Hammersley(uint i, uint N);
float GeoSmith(float NdotV, float NdotL, float roughness);

const float PI = 3.141592653589793;

vec3 BuildViewVector(float cosT) {
    return vec3(sqrt(1.0 - cosT * cosT), 0.0, cosT);
}

vec2 IntegrateBRDF(float NdotV, float roughness) {
    vec3 V = BuildViewVector(NdotV);

    float I1 = 0.0;
    float I2 = 0.0;

    vec3 N = vec3(0.0, 0.0, 1.0);
    for (uint i = 0u; i < numSamples; ++i) {
        vec2 Xi = Hammersley(i, numSamples);
        vec3 H = SampleGGX(Xi, N, roughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = clamp(L.z, 0.0, 1.0);
        float NdotH = clamp(H.z, 0.0, 1.0);
        float VdotH = clamp(dot(V, H), 0.0, 1.0);

        if (NdotL > 0.0) {
            float G = GeoSmith(NdotV, NdotL, roughness);
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

    I1 /= float(numSamples);
    I2 /= float(numSamples);

    return vec2(I1, I2);
}

void main() {
    FragColor = IntegrateBRDF(fsTexCoords.x, fsTexCoords.y);
}