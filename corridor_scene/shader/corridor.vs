#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 FragPosLightSpace0;
    vec4 FragPosLightSpace1;
    vec4 FragPosLightSpace2;
} vs_out;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix0;
uniform mat4 lightSpaceMatrix1;
uniform mat4 lightSpaceMatrix2;

void main()
{
    vs_out.FragPos            = vec3(model * vec4(aPos, 1.0));
    vs_out.Normal             = mat3(transpose(inverse(model))) * aNormal;
    vs_out.TexCoords          = aTexCoords;
    vs_out.FragPosLightSpace0 = lightSpaceMatrix0 * vec4(vs_out.FragPos, 1.0);
    vs_out.FragPosLightSpace1 = lightSpaceMatrix1 * vec4(vs_out.FragPos, 1.0);
    vs_out.FragPosLightSpace2 = lightSpaceMatrix2 * vec4(vs_out.FragPos, 1.0);
    gl_Position = projection * view * vec4(vs_out.FragPos, 1.0);
}
