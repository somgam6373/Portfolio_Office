#version 330 core
out vec4 FragColor;
in  vec2 TexCoords;

uniform sampler2D scene;      // original HDR scene
uniform sampler2D bloomBlur;  // blurred bright mask
uniform float     exposure;
uniform float     bloomStrength;

void main()
{
    vec3 hdrColor   = texture(scene,     TexCoords).rgb;
    vec3 bloomColor = texture(bloomBlur, TexCoords).rgb;

    // Additive bloom
    hdrColor += bloomColor * bloomStrength;

    // Reinhard-style exposure tone mapping
    vec3 result = vec3(1.0) - exp(-hdrColor * exposure);

    // Gamma correction
    result = pow(result, vec3(1.0 / 2.2));

    FragColor = vec4(result, 1.0);
}
