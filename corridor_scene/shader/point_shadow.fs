#version 330 core
in vec4 FragPos;

uniform vec3  lightPos;
uniform float farPlane;

void main()
{
    float lightDist = length(FragPos.xyz - lightPos);
    gl_FragDepth = lightDist / farPlane;
}
