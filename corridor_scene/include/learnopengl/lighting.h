#ifndef LIGHTING_H
#define LIGHTING_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <learnopengl/shader.h>
#include <string>
#include <ctime>

struct PointLight
{
    glm::vec3 position;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    float constant;
    float linear;
    float quadratic;
    bool enabled;
};

struct DirLight
{
    glm::vec3 direction;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    bool      enabled = true;
};

// Sunlight entering through the back window (z=-6.75 wall)
DirLight CalcTimeDirLight()
{
    DirLight light;
    light.direction = glm::vec3(0.10f, -0.50f, 0.85f);  // from window into room, slightly downward
    light.ambient   = glm::vec3(0.30f, 0.29f, 0.26f);   // warm ambient
    light.diffuse   = glm::vec3(1.08f, 1.02f, 0.84f);   // warm sunlight
    light.specular  = glm::vec3(0.85f);
    return light;
}

void SetPointLightUniforms(Shader& shader, const PointLight& light)
{
    // when disabled: cut diffuse/specular, keep a minimal ambient so the room is not pitch black
    shader.setVec3("pointLight.position",  light.position);
    shader.setVec3("pointLight.ambient",   light.enabled ? light.ambient  : glm::vec3(0.02f));
    shader.setVec3("pointLight.diffuse",   light.enabled ? light.diffuse  : glm::vec3(0.0f));
    shader.setVec3("pointLight.specular",  light.enabled ? light.specular : glm::vec3(0.0f));
    shader.setFloat("pointLight.constant",  light.constant);
    shader.setFloat("pointLight.linear",    light.linear);
    shader.setFloat("pointLight.quadratic", light.quadratic);
}

void SetNamedPointLightUniforms(Shader& shader, const PointLight& light, const std::string& name)
{
    shader.setVec3(name + ".position",  light.position);
    shader.setVec3(name + ".ambient",   light.enabled ? light.ambient  : glm::vec3(0.02f));
    shader.setVec3(name + ".diffuse",   light.enabled ? light.diffuse  : glm::vec3(0.0f));
    shader.setVec3(name + ".specular",  light.enabled ? light.specular : glm::vec3(0.0f));
    shader.setFloat(name + ".constant",  light.constant);
    shader.setFloat(name + ".linear",    light.linear);
    shader.setFloat(name + ".quadratic", light.quadratic);
}

void SetDirLightUniforms(Shader& shader, const DirLight& light)
{
    shader.setVec3("dirLight.direction", light.direction);
    shader.setVec3("dirLight.ambient",   light.enabled ? light.ambient  : glm::vec3(0.0f));
    shader.setVec3("dirLight.diffuse",   light.enabled ? light.diffuse  : glm::vec3(0.0f));
    shader.setVec3("dirLight.specular",  light.enabled ? light.specular : glm::vec3(0.0f));
}

#endif
