#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D hdrBuffer;
uniform float     exposure;

void main()
{
    vec3 hdrColor = texture(hdrBuffer, TexCoords).rgb;

    // Reinhard 톤 매핑: exposure 조정 후 [0,∞) → [0,1) 압축
    vec3 tone   = hdrColor * exposure;
    vec3 mapped = tone / (tone + vec3(1.0));

    // 감마 인코딩: 선형 → sRGB (γ = 2.2)
    mapped = pow(mapped, vec3(1.0 / 2.2));
    FragColor = vec4(mapped, 1.0);
}
