#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTex;
uniform float     time;       // glfwGetTime()
uniform float     intensity;  // 0.0 = off, 1.0 = full drunk

void main()
{
    vec2 uv = TexCoords;

    // ── Screen shake: slow sinusoidal offset ──────────────────
    uv.x += sin(time * 2.3 + uv.y * 4.0) * 0.007 * intensity;
    uv.y += cos(time * 1.7 + uv.x * 3.5) * 0.005 * intensity;

    // ── Barrel / rolling-wave distortion ─────────────────────
    uv.x += sin(uv.y * 6.0 + time * 1.4) * 0.010 * intensity;
    uv.y += cos(uv.x * 5.0 + time * 1.1) * 0.007 * intensity;

    uv = clamp(uv, 0.001, 0.999);

    // ── Chromatic aberration: R / G / B at offset UVs ────────
    float ca = 0.006 * intensity;
    float r  = texture(screenTex, uv + vec2( ca,  0.0)).r;
    float g  = texture(screenTex, uv              ).g;
    float b  = texture(screenTex, uv - vec2( ca,  0.0)).b;

    // ── Vignette darkening at screen edges ────────────────────
    vec2  c    = uv - 0.5;
    float vign = 1.0 - dot(c, c) * 1.8 * intensity;
    vign = clamp(vign, 0.0, 1.0);

    FragColor = vec4(vec3(r, g, b) * vign, 1.0);
}
