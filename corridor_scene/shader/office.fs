#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec3 Tangent;
in vec2 TexCoords;
in vec4 FragPosLightSpace;

struct PointLight
{
    vec3  position;
    vec3  ambient;
    vec3  diffuse;
    vec3  specular;
    float constant;
    float linear;
    float quadratic;
};

struct DirLight
{
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform PointLight pointLight;
uniform PointLight lampLight1;
uniform PointLight lampLight2;
uniform DirLight   dirLight;
uniform vec3       viewPos;
uniform float      shininess;
uniform sampler2D  texture_diffuse1;
uniform sampler2D  texture_specular1;
uniform sampler2D  texture_normal1;
uniform bool       hasDiffuseTex;
uniform bool       hasNormalTex;
uniform vec3       uDiffuseColor;
uniform vec3       uEmissiveColor;      // flat emission color (set per-object, e.g. warm white)
uniform float      uEmissiveStrength;   // 0=off, >0=self-illuminated
uniform bool       uEmissiveAllSides;   // true: all normals glow; false: downward-only (ceiling dome)
uniform sampler2D  shadowMap;
uniform samplerCube shadowCubeMap;
uniform float       shadowFarPlane;
uniform vec3        shadowPointPos;

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir,
                  vec3 diffTex, vec3 specTex)
{
    vec3  lightDir  = normalize(-light.direction);
    float diff      = max(dot(normal, lightDir), 0.0);
    vec3  halfDir   = normalize(lightDir + viewDir);
    float spec      = pow(max(dot(normal, halfDir), 0.0), shininess);

    return light.ambient  * diffTex
         + light.diffuse  * diff * diffTex
         + light.specular * spec * specTex;
}

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir,
                    vec3 diffTex, vec3 specTex)
{
    vec3  lightDir    = normalize(light.position - fragPos);
    float diff        = max(dot(normal, lightDir), 0.0);
    vec3  halfDir     = normalize(lightDir + viewDir);
    float spec        = pow(max(dot(normal, halfDir), 0.0), shininess);
    float dist        = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant
                              + light.linear    * dist
                              + light.quadratic * dist * dist);

    vec3 result = light.ambient  * diffTex
                + light.diffuse  * diff * diffTex
                + light.specular * spec * specTex;
    return result * attenuation;
}

float ShadowCalcPoint(vec3 fragPos)
{
    vec3 fragToLight    = fragPos - shadowPointPos;
    float currentDepth  = length(fragToLight);

    vec3 sampleDirs[20] = vec3[](
        vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1),
        vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
        vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
        vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
        vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
    );

    float shadow     = 0.0;
    float bias       = 0.12;
    float viewDist   = length(viewPos - fragPos);
    float diskRadius = (1.0 + viewDist / shadowFarPlane) / 25.0;

    for (int i = 0; i < 20; ++i)
    {
        float closestDist = texture(shadowCubeMap, fragToLight + sampleDirs[i] * diskRadius).r
                          * shadowFarPlane;
        if (currentDepth - bias > closestDist)
            shadow += 1.0;
    }
    return shadow / 20.0;
}

float ShadowCalc(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0)
        return 0.0;
    float currentDepth = projCoords.z;
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.001);
    // PCF 3x3
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    shadow /= 9.0;
    return shadow;
}

void main()
{
    vec3 N = normalize(Normal);
    vec3 T = normalize(Tangent - dot(Tangent, N) * N);
    vec3 B = cross(N, T);
    mat3 TBN = mat3(T, B, N);

    vec3 norm;
    if (hasNormalTex)
    {
        vec3 normalSample = texture(texture_normal1, TexCoords).rgb * 2.0 - 1.0;
        norm = normalize(TBN * normalSample);
    }
    else
        norm = N;

    // sRGB 텍스처를 선형 공간으로 변환 (감마 보정 파이프라인)
    vec3 diffTex = hasDiffuseTex
        ? pow(texture(texture_diffuse1, TexCoords).rgb, vec3(2.2))
        : pow(uDiffuseColor, vec3(2.2));
    vec3 specTex = texture(texture_specular1, TexCoords).rgb;  // 스펙큘러맵은 이미 선형

    vec3 viewDir = normalize(viewPos - FragPos);

    // directional light with shadow (Blinn-Phong)
    vec3  lightDirN  = normalize(-dirLight.direction);
    float shadow     = ShadowCalc(FragPosLightSpace, norm, lightDirN);
    float diff       = max(dot(norm, lightDirN), 0.0);
    vec3  halfDirN   = normalize(lightDirN + viewDir);
    float spec       = pow(max(dot(norm, halfDirN), 0.0), shininess);
    // hemisphere ambient: upward faces get full ambient, downward faces attenuated
    float hemiT      = 0.5 + 0.5 * norm.y;
    vec3  dirAmbient  = dirLight.ambient * mix(0.2, 1.0, hemiT) * diffTex;
    vec3  dirDiffSpec = dirLight.diffuse  * diff * diffTex
                      + dirLight.specular * spec * specTex;
    vec3 result = dirAmbient * (1.0 - shadow * 0.5) + (1.0 - shadow) * dirDiffSpec;

    float ptShadow = ShadowCalcPoint(FragPos);
    result += CalcPointLight(pointLight,  norm, FragPos, viewDir, diffTex, specTex) * (1.0 - ptShadow * 0.85);
    result += CalcPointLight(lampLight1,  norm, FragPos, viewDir, diffTex, specTex);
    result += CalcPointLight(lampLight2,  norm, FragPos, viewDir, diffTex, specTex);
    // uEmissiveAllSides=false: downward-facing only (ceiling dome interior)
    // uEmissiveAllSides=true : all surfaces (floor lamp shade — shade glows on every face)
    float emissiveFront = uEmissiveAllSides ? 1.0 : max(0.0, -norm.y);
    result += uEmissiveColor * uEmissiveStrength * emissiveFront;

    // HDR FBO(GL_RGBA16F)에 선형 값 그대로 출력 — 감마 인코딩은 hdr.fs 톤매핑 패스에서 처리
    FragColor = vec4(max(result, vec3(0.0)), 1.0);
}
