const float PI = 3.141592653589793;

// ----------------------------------------------------------------------------
// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
// efficient VanDerCorpus calculation.
float RadicalInverseVdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;  // / 0x100000000
}

vec2 Hammersley(uint i, uint N) {
    return vec2(float(i) / float(N), RadicalInverseVdC(i));
}

vec2 SphericalUVMap(vec3 w) {
    vec2 uv = vec2(atan(w.z, w.x) / (2 * PI),
                   asin(w.y) / PI);
    uv += vec2(0.5, 0.5);

    return vec2(1 - uv.x, 1 - uv.y);
}

// Build basis from N and convert Wi
vec3 TangentToWorld(vec3 Wi, vec3 N) {
    vec3 Up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 Tan = normalize(cross(Up, N));
    vec3 Bitan = cross(N, Tan);

    vec3 WorldVec = Tan * Wi.x + Bitan * Wi.y + N * Wi.z;

    return normalize(WorldVec);
}

vec3 SampleCosWeightedHemis(vec2 Xi, vec3 N) {
    float phi = 2 * PI * Xi.x;
    float cosTheta = sqrt(Xi.y);
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    vec3 Wi = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

    return TangentToWorld(Wi, N);
}

vec3 SampleGGX(vec2 Xi, vec3 N, float rough) {
    float a = rough * rough;

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (Xi.y * (a * a - 1.0) + 1));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

    return TangentToWorld(H, N);
}

float DistGGX(vec3 N, vec3 H, float rough) {
    float a = rough * rough;
    float a2 = a * a;

    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    
    return a2 / (PI * denom * denom);
}

float GeoSmith(float NdotV, float NdotL, float rough) {
    float a = rough * rough;
    float a2 = a * a;

    float NdotL2 = NdotL * NdotL;
    float NdotV2 = NdotV * NdotV;

    float GGX1 = NdotV * sqrt((NdotL2 - NdotL2 * a2) + a2);
    float GGX2 = NdotL * sqrt((NdotV2 - NdotV2 * a2) + a2);

    return 0.5 / (GGX1 + GGX2);
}