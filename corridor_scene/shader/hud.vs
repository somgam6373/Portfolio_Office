#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;

out vec3 FragPos;
out vec3 Normal;
out vec3 Tangent;
out vec2 TexCoords;
out vec4 FragPosLightSpace;

uniform mat4 hudProjection;
uniform mat4 hudModel;
uniform mat4 hudInvView;        // inverse of main scene view — converts HUD space to world space
uniform mat4 lightSpaceMatrix;

void main()
{
    // hudModel positions vertices in camera-relative (view) space.
    // Multiply by inverse view to get world-space coordinates for lighting.
    vec4 worldPos     = hudInvView * hudModel * vec4(aPos, 1.0);
    mat3 normalMatrix = mat3(transpose(inverse(hudInvView * hudModel)));

    FragPos           = vec3(worldPos);
    Normal            = normalMatrix * aNormal;
    Tangent           = normalMatrix * aTangent;
    TexCoords         = aTexCoords;
    FragPosLightSpace = lightSpaceMatrix * worldPos;

    gl_Position = hudProjection * hudModel * vec4(aPos, 1.0);
}
