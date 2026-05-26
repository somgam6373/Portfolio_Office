#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform vec3      emissiveColor;
uniform float     emissiveAlpha;
uniform sampler2D emissiveTex;
uniform int       useTexture;   // 0 = solid color, 1 = sample texture

void main()
{
    vec3 col = (useTexture == 1)
        ? texture(emissiveTex, TexCoords).rgb
        : emissiveColor;
    FragColor = vec4(col, emissiveAlpha);
}
