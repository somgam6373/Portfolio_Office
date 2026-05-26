#version 330 core
// Normal-Mapped Wall Vertex Shader
// Ref: learnopengl.com/Advanced-Lighting/Normal-Mapping
//
// Key idea: transform ALL lighting vectors into tangent space once per vertex
// so the fragment shader just does cheap dot products against the normal map.

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;

out VS_OUT {
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
} vs_out;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix0;
uniform mat4 lightSpaceMatrix1;
uniform mat4 lightSpaceMatrix2;

uniform vec3 lampPositions[3];
uniform vec3 doorLightPos;
uniform vec3 viewPos;

void main()
{
    vec3 fragPos = vec3(model * vec4(aPos, 1.0));
    vs_out.FragPos            = fragPos;
    vs_out.TexCoords          = aTexCoords;
    vs_out.FragPosLightSpace0 = lightSpaceMatrix0 * vec4(fragPos, 1.0);
    vs_out.FragPosLightSpace1 = lightSpaceMatrix1 * vec4(fragPos, 1.0);
    vs_out.FragPosLightSpace2 = lightSpaceMatrix2 * vec4(fragPos, 1.0);

    // --- Build TBN in world space ---
    // normal matrix = transpose(inverse(model)) for non-uniform scale safety
    mat3 nm = transpose(inverse(mat3(model)));
    vec3 T   = normalize(nm * aTangent);
    vec3 N   = normalize(nm * aNormal);

    // Gram-Schmidt re-orthogonalisation: removes accumulated float drift
    // and ensures T is exactly perpendicular to N
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);           // recompute B from orthogonal T and N

    // TBN: columns are world-space T, B, N
    // transpose(TBN) == TBN^-1 for orthonormal basis  ->  world->tangent transform
    mat3 TBNinv = transpose(mat3(T, B, N));

    // Transform all positions to tangent space here (per-vertex, cheap)
    for (int i = 0; i < 3; ++i)
        vs_out.TangentLampPos[i]  = TBNinv * lampPositions[i];
    vs_out.TangentDoorLightPos    = TBNinv * doorLightPos;
    vs_out.TangentViewPos         = TBNinv * viewPos;
    vs_out.TangentFragPos         = TBNinv * fragPos;

    gl_Position = projection * view * vec4(fragPos, 1.0);
}
