#version 330 core
out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 FragPosLightSpace0;
    vec4 FragPosLightSpace1;
    vec4 FragPosLightSpace2;
} fs_in;

uniform sampler2D diffuseTex;

uniform vec3 lampPositions[3];   // three ceiling lamps
uniform vec3 lampColor;
uniform vec3 doorLightPos;
uniform vec3 doorLightColor;
uniform vec3 viewPos;
uniform vec3 colorTint;

vec3 calcPointLight(vec3 lPos, vec3 lColor, vec3 norm, vec3 fragPos, vec3 viewDir, float shadowFactor)
{
    float dist = length(lPos - fragPos);
    float att  = 1.0 / (1.0 + 0.14*dist + 0.07*dist*dist);

    vec3  lightDir = normalize(lPos - fragPos);
    float diff     = max(dot(norm, lightDir), 0.0);
    vec3  diffuse  = diff * lColor * att * (1.0 - shadowFactor * 0.80);

    vec3  halfway  = normalize(lightDir + viewDir);
    float spec     = pow(max(dot(norm, halfway), 0.0), 32.0);
    vec3  specular = 0.06 * spec * lColor * att * (1.0 - shadowFactor);

    return diffuse + specular;
}

void main()
{
    vec3 texColor = texture(diffuseTex, fs_in.TexCoords).rgb * colorTint;
    vec3 norm     = normalize(fs_in.Normal);
    vec3 viewDir  = normalize(viewPos - fs_in.FragPos);

    vec3 ambient = 0.18 * lampColor * texColor;

    vec3 lighting = vec3(0.0);
    lighting += calcPointLight(lampPositions[0], lampColor, norm, fs_in.FragPos, viewDir, 0.0);
    lighting += calcPointLight(lampPositions[1], lampColor, norm, fs_in.FragPos, viewDir, 0.0);
    lighting += calcPointLight(lampPositions[2], lampColor, norm, fs_in.FragPos, viewDir, 0.0);

    // Cool fill light from door end (no shadow)
    lighting += calcPointLight(doorLightPos, doorLightColor, norm, fs_in.FragPos, viewDir, 0.0) * 0.25;

    vec3 result = (ambient + lighting) * texColor;
    result = max(result, texColor * 0.03);

    FragColor = vec4(result, 1.0);
}
