#ifndef SCENE_H
#define SCENE_H

// scene transition is handled by the other team member -- this file is the merge interface stub
enum SceneType
{
    SCENE_CORRIDOR,  // corridor scene (to be implemented by other team member)
    SCENE_OFFICE     // office scene (implemented in this file)
};

struct SceneState
{
    SceneType current = SCENE_OFFICE;
};

#endif
