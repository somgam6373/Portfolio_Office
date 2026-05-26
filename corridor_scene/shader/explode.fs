#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform vec3  colorTint;
uniform float explodeTime;

void main()
{
    // Simple overhead directional light — no shadow needed for flying shards
    vec3  n    = normalize(Normal);
    vec3  L    = normalize(vec3(0.0, 1.0, 0.4));
    float diff = max(dot(n, L), 0.0);
    vec3  col  = colorTint * (0.35 + 0.65 * diff);

    // Alpha fades to 0 as explosion progresses (full fade at ~1.8 s)
    float alpha = max(0.0, 1.0 - explodeTime * 0.55);

    FragColor = vec4(col, alpha);
}
