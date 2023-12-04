const float PI = 3.141592653589793;

vec2 SphericalUVMap(vec3 w) {
    vec2 uv = vec2(atan(w.z, w.x) / (2 * PI),
                   asin(w.y) / PI);
    uv += vec2(0.5, 0.5);
    return vec2(1 - uv.x, 1 - uv.y);
}

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

vec3 SampleGGX(vec2 Xi, vec3 N, float rough) {
    float a = rough * rough;

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (Xi.y * (a * a - 1.0) + 1));
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