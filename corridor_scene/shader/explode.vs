#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;

// Passed to geometry shader via interface block
out VS_OUT {
    vec3 worldPos;   // world-space position for GS displacement
    vec3 normal;     // world-space normal
    vec2 texCoords;
} vs_out;

uniform mat4 model;

void main()
{
    vec4 world        = model * vec4(aPos, 1.0);
    vs_out.worldPos   = vec3(world);
    vs_out.normal     = mat3(transpose(inverse(model))) * aNormal;
    vs_out.texCoords  = aTexCoords;
    // GS applies view * proj; pass world pos as a placeholder
    gl_Position = world;
}
