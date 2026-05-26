#version 330 core
// Normal-Mapped Wall Fragment Shader  (Blinn-Phong + per-lamp PCF shadow)
// Ref: learnopengl.com/Advanced-Lighting/Normal-Mapping
//
// All lighting is computed in tangent space using the normal fetched
// from the normal map.  No world-space normal matrix needed per fragment.
// Each of the three ceiling lamps has its own shadow map so objects block
// each light source independently.

out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec2 TexCoords;
    vec4 FragPosLightSpace0;
    vec4 FragPosLightSpace1;
    vec4 FragPosLightSpace2;
    // All light/view vectors pre-transformed to tangent space
    vec3 TangentLampPos[3];
    vec3 TangentDoorLightPos;
    vec3 TangentViewPos;
    vec3 TangentFragPos;
} fs_in;

uniform sampler2D diffuseTex;   // unit 0 - wall albedo
uniform sampler2D normalMap;    // unit 1 - tangent-space normals (RGB)

uniform vec3 lampColor;
uniform vec3 doorLightColor;
uniform vec3 colorTint;

// ---------- One Blinn-Phong point-light contribution ----------
// All inputs are already in tangent space.
vec3 pointLight(vec3 N, vec3 V, vec3 tangentLightPos,
                vec3 tangentFragPos, vec3 lightColor,
                float kLin, float kQuad,
                vec3 albedo, float specStrength)
{
    vec3  L     = normalize(tangentLightPos - tangentFragPos);
    float dist  = length(tangentLightPos - tangentFragPos);
    float atten = 1.0 / (1.0 + kLin * dist + kQuad * dist * dist);

    // Diffuse (Lambertian)
    float diff  = max(dot(N, L), 0.0);

    // Specular (Blinn-Phong half-vector)
    vec3  H     = normalize(L + V);
    float spec  = pow(max(dot(N, H), 0.0), 64.0) * specStrength;

    return lightColor * atten * (diff * albedo + vec3(spec));
}

void main()
{
    // --- Fetch and unpack normal map ---
    // Normal stored in [0,1]; unpack to [-1,1] tangent-space direction.
    // NOTE: do NOT gamma-correct normal maps — they are already linear data.
    vec3 N = normalize(texture(normalMap, fs_in.TexCoords).rgb * 2.0 - 1.0);

    vec3 V      = normalize(fs_in.TangentViewPos - fs_in.TangentFragPos);
    vec3 albedo = texture(diffuseTex, fs_in.TexCoords).rgb * colorTint;

    // --- Accumulate 3 ceiling lamps ---
    vec3 Lo = vec3(0.0);
    Lo += pointLight(N, V, fs_in.TangentLampPos[0], fs_in.TangentFragPos,
                     lampColor, 0.14, 0.07, albedo, 0.22);
    Lo += pointLight(N, V, fs_in.TangentLampPos[1], fs_in.TangentFragPos,
                     lampColor, 0.14, 0.07, albedo, 0.22);
    Lo += pointLight(N, V, fs_in.TangentLampPos[2], fs_in.TangentFragPos,
                     lampColor, 0.14, 0.07, albedo, 0.22);

    // --- Door fill light (cool-tinted, wider falloff, no shadow) ---
    Lo += pointLight(N, V,
                     fs_in.TangentDoorLightPos, fs_in.TangentFragPos,
                     doorLightColor, 0.22, 0.20, albedo, 0.15);

    // --- Combine: ambient + shadowed diffuse+specular ---
    vec3 ambient = 0.18 * lampColor * albedo;
    vec3 color   = ambient + Lo;

    // Prevent pitch-black areas
    color = max(color, albedo * 0.03);

    FragColor = vec4(color, 1.0);
}
