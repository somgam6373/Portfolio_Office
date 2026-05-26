#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

// --- Material uniforms ---
uniform vec3  albedo;
uniform float metallic;
uniform float roughness;
uniform float ao;

// --- Point lights (up to 4) ---
uniform vec3 lightPositions[4];
uniform vec3 lightColors[4];
uniform int  numLights;

uniform vec3 viewPos;

const float PI = 3.14159265359;

// ---- Cook-Torrance components ----

// GGX Normal Distribution Function
float DistributionGGX(vec3 N, vec3 H, float r)
{
    float a  = r * r;
    float a2 = a * a;
    float d  = max(dot(N, H), 0.0);
    float d2 = d * d;
    float denom = (d2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

// Smith's Schlick-GGX Geometry term (single direction)
float GeometrySchlickGGX(float NdotV, float r)
{
    float k = (r + 1.0) * (r + 1.0) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

// Smith Geometry function (both view and light direction)
float GeometrySmith(vec3 N, vec3 V, vec3 L, float r)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, r) * GeometrySchlickGGX(NdotL, r);
}

// Schlick Fresnel approximation
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ---- Main ----
void main()
{
    vec3 N = normalize(Normal);
    vec3 V = normalize(viewPos - FragPos);

    // F0: base reflectance
    // Dielectric = 0.04, Metal = albedo
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = vec3(0.0);

    for (int i = 0; i < numLights; ++i)
    {
        vec3  L         = normalize(lightPositions[i] - FragPos);
        vec3  H         = normalize(V + L);
        float dist      = length(lightPositions[i] - FragPos);
        float atten     = 1.0 / (dist * dist);
        vec3  radiance  = lightColors[i] * atten;

        // --- BRDF ---
        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3  num   = NDF * G * F;
        float denom = 4.0 * max(dot(N,V), 0.0) * max(dot(N,L), 0.0) + 0.0001;
        vec3  spec  = num / denom;

        // Energy conservation: kS = F, kD = rest
        vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);

        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + spec) * radiance * NdotL;
    }

    // Simple ambient term
    vec3 ambient = vec3(0.06) * albedo * ao;
    vec3 color   = ambient + Lo;

    // Output raw HDR value — bloom_final.fs applies tone mapping
    FragColor = vec4(color, 1.0);
}
