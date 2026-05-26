#ifndef RAY_CASTER_H
#define RAY_CASTER_H

#include <glm/glm.hpp>
#include <learnopengl/camera.h>

struct Ray
{
    glm::vec3 origin;
    glm::vec3 direction;
};

struct AABB
{
    glm::vec3 min;
    glm::vec3 max;
};

// Slab method: returns true and sets tHit to ray entry distance on intersection.
bool RayAABBIntersect(const Ray& ray, const AABB& box, float& tHit)
{
    glm::vec3 invDir = 1.0f / ray.direction;
    glm::vec3 t0 = (box.min - ray.origin) * invDir;
    glm::vec3 t1 = (box.max - ray.origin) * invDir;

    glm::vec3 tMin = glm::min(t0, t1);
    glm::vec3 tMax = glm::max(t0, t1);

    float tEnter = glm::max(glm::max(tMin.x, tMin.y), tMin.z);
    float tExit  = glm::min(glm::min(tMax.x, tMax.y), tMax.z);

    if (tExit < 0.0f || tEnter > tExit)
        return false;

    tHit = tEnter;
    return true;
}

// build a picking ray from camera position along camera front vector
Ray MakePickingRay(const Camera& camera)
{
    Ray ray;
    ray.origin    = camera.Position;
    ray.direction = glm::normalize(camera.Front);
    return ray;
}

#endif
