#version 330 core
out vec4 FragColor;

in  vec2 TexCoords;        // emissive.vs output — declared, not used below

uniform sampler2D mirrorTex;
uniform vec2      screenSize;  // (SCR_WIDTH, SCR_HEIGHT)

void main()
{
    // Each mirror-surface pixel samples the reflection texture at its own
    // screen-space position.  The reflection was rendered from the reflected
    // camera at the same resolution, so the mapping is exact.
    vec2 uv = gl_FragCoord.xy / screenSize;
    FragColor = vec4(texture(mirrorTex, uv).rgb, 1.0);
}
