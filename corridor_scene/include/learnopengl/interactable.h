#ifndef INTERACTABLE_H
#define INTERACTABLE_H

#include <learnopengl/ray_caster.h>
#include <string>
#include <vector>

#ifdef _WIN32
#include <cstdlib>  // system()
#endif

const float HOVER_REQUIRED = 3.0f;  // seconds of continuous hover required to open URL

struct InteractableObject
{
    AABB        bounds;
    std::string label;
    std::string url;
    float       hoverTimer = 0.0f;
    bool        triggered  = false;
};

void OpenURL(const std::string& url)
{
    if (url.empty()) return;
#ifdef _WIN32
    std::string cmd = "start " + url;
    system(cmd.c_str());
#endif
}

// detect the closest object hit by ray and update hover timers.
// returns the index of the currently hovered object, or -1 if none.
int UpdateInteractables(std::vector<InteractableObject>& objects,
                        const Ray& ray, float maxDist, float deltaTime)
{
    int   hitIndex = -1;
    float closestT = maxDist;

    for (int i = 0; i < (int)objects.size(); i++)
    {
        float t;
        if (RayAABBIntersect(ray, objects[i].bounds, t) && t < closestT)
        {
            closestT = t;
            hitIndex = i;
        }
    }

    for (int i = 0; i < (int)objects.size(); i++)
    {
        if (i == hitIndex)
        {
            objects[i].hoverTimer += deltaTime;
            if (objects[i].hoverTimer >= HOVER_REQUIRED && !objects[i].triggered)
            {
                objects[i].triggered = true;
                OpenURL(objects[i].url);
            }
        }
        else
        {
            objects[i].hoverTimer = 0.0f;
            objects[i].triggered  = false;
        }
    }

    return hitIndex;
}

#endif
