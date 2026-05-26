#version 330 core
layout (location = 0) in vec2 aPos;

void main()
{
    // Full-screen quad in NDC, no transform needed
    gl_Position = vec4(aPos, 0.0, 1.0);
}
