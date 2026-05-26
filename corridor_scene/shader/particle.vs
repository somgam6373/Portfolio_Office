#version 330 core
layout (location = 0) in vec3  aPos;
layout (location = 1) in vec2  aTexCoords;
layout (location = 2) in float aAlpha;

uniform mat4 view;
uniform mat4 projection;

out vec2  TexCoords;
out float Alpha;

void main()
{
    TexCoords   = aTexCoords;
    Alpha       = aAlpha;
    gl_Position = projection * view * vec4(aPos, 1.0);
}
