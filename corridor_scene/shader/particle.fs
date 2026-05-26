#version 330 core
out vec4 FragColor;

in vec2  TexCoords;
in float Alpha;

uniform vec3 particleColor;

void main()
{
    // Circular soft glow: distance from center of the quad
    vec2  uv = TexCoords - vec2(0.5);
    float d  = length(uv);
    if (d > 0.5) discard;

    float glow = 1.0 - (d * 2.0);
    glow = glow * glow;              // quadratic falloff for soft edge

    FragColor = vec4(particleColor, glow * Alpha);
}
