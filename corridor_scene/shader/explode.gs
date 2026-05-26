#version 330 core
layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

// From vertex shader
in VS_OUT {
    vec3 worldPos;
    vec3 normal;
    vec2 texCoords;
} gs_in[];

// To fragment shader
out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;

uniform mat4  view;
uniform mat4  projection;
uniform float explodeTime;   // 0.0 = intact; increases after trigger

void main()
{
    // Per-face outward normal (world space) from triangle edges
    vec3 e1    = gs_in[1].worldPos - gs_in[0].worldPos;
    vec3 e2    = gs_in[2].worldPos - gs_in[0].worldPos;
    vec3 faceN = normalize(cross(e1, e2));

    float t    = explodeTime;
    float disp = t * t * 1.8;   // quadratic outward movement
    float grav = t * t * 2.8;   // downward gravity pull

    for (int i = 0; i < 3; i++)
    {
        vec3 pos = gs_in[i].worldPos + faceN * disp;
        pos.y   -= grav;

        gl_Position = projection * view * vec4(pos, 1.0);
        FragPos     = pos;
        Normal      = gs_in[i].normal;
        TexCoords   = gs_in[i].texCoords;
        EmitVertex();
    }
    EndPrimitive();
}
