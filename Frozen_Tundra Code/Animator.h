#pragma once
#ifndef ANIMATOR_H
#define ANIMATOR_H

#include <vector>
#include <map>
#include <string>
#include <assimp/scene.h>
#include "glm/glm.hpp"
#include "Animation.h"
#include "Bone.h"
#pragma once


class Animator
{
public:
    Animator(Animation* animation);

    void UpdateAnimation(float dt);
    void PlayAnimation(Animation* pAnimation);
    void CalculateBoneTransform(const AssimpNodeData* node, glm::mat4 parentTransform);
    std::vector<glm::mat4>& GetFinalBoneMatrices();

    float m_AnimationSpeed;

    // [已修改] 再次降低默认行走速度，使其更沉重
    float m_WalkSpeed;
  
private:
    std::vector<glm::mat4> m_FinalBoneMatrices;
    Animation* m_CurrentAnimation;
    float m_CurrentTime;
    float m_DeltaTime;

    // m_WalkCycle 现在是一个 0.0 -> 1.0 的循环
    float m_WalkCycle;
};
#endif