#include "Animator.h"
#include "glm/gtc/matrix_transform.hpp" 
#include <cmath> 
#include <algorithm> 
#include <iostream> 

// 定义 M_PI
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
const float TWO_PI = 2.0f * (float)M_PI;

float lerp(float a, float b, float f)
{
    return a + f * (b - a);
}

Animator::Animator(Animation* animation)
{
    m_CurrentTime = 0.0;
    m_CurrentAnimation = animation;
    m_AnimationSpeed = 0.3f;
    m_WalkSpeed = 0.3f;
    m_WalkCycle = 0.0f;

    m_FinalBoneMatrices.reserve(100);
    for (int i = 0; i < 100; i++)
        m_FinalBoneMatrices.push_back(glm::mat4(1.0f));
}

void Animator::UpdateAnimation(float dt)
{
    m_DeltaTime = dt;
    if (m_CurrentAnimation)
    {
        m_CurrentTime += dt * m_AnimationSpeed;
        m_WalkCycle = fmod(m_WalkCycle + dt * m_WalkSpeed, 1.0f);
        CalculateBoneTransform(&m_CurrentAnimation->GetRootNode(), glm::mat4(1.0f));
    }
}

void Animator::PlayAnimation(Animation* pAnimation)
{
    m_CurrentAnimation = pAnimation;
    m_CurrentTime = 0.0f;
    m_WalkCycle = 0.0f;
}

// =========================================================
// [修改] 'CalculateBoneTransform' (层级动画版)
// =========================================================
void Animator::CalculateBoneTransform(const AssimpNodeData* node, glm::mat4 parentTransform)
{
    std::string nodeName = node->name;
    glm::mat4 nodeTransform = node->transformation;

    // --- 1. 程序化动画逻辑 (保持不变) ---

    float rightPhase = fmod(m_WalkCycle, 1.0f);
    float leftPhase = fmod(m_WalkCycle + 0.5f, 1.0f);
    float cycleRad = m_WalkCycle * TWO_PI;

    // A. 身体起伏
    float bobCycle = fmod(m_WalkCycle + 0.25f, 1.0f);
    float bodyBob = (cos(bobCycle * TWO_PI) * 0.5f + 0.5f) * -0.4f;

    // B. 臀部运动
    float hipSway = -cos(cycleRad) * glm::radians(4.0f);
    float hipTilt = sin(cycleRad) * glm::radians(4.0f);

    // C. 脊柱运动
    float spineSway = -hipSway * 1.5f;
    float spineTilt = -hipTilt * 1.2f;

    // --- 2. 应用变换 ---

    // [骨盆/根]
    if (nodeName == "Bip01_Pelvis")
    {
        nodeTransform = nodeTransform *
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, bodyBob, 0.0f)) * glm::rotate(glm::mat4(1.0f), hipSway, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::rotate(glm::mat4(1.0f), hipTilt, glm::vec3(0.0f, 0.0f, 1.0f));
    }

    // [脊柱]
    float spineHunch = -bodyBob * glm::radians(5.0f);
    if (nodeName == "Bip01_Spine" || nodeName == "Bip01_Spine1") {
        nodeTransform = nodeTransform *
            glm::rotate(glm::mat4(1.0f), spineSway, glm::vec3(0.0f, 1.0f, 0.0f)) *
            glm::rotate(glm::mat4(1.0f), spineTilt, glm::vec3(0.0f, 0.0f, 1.0f)) *
            glm::rotate(glm::mat4(1.0f), spineHunch, glm::vec3(0.0f, 0.0f, 1.0f));
    }

    // [头部]
    if (nodeName == "Bip01_Head" || nodeName == "Bip01_Neck") {
        float headNod = -spineHunch * 0.5f;
        float headCounterSway = -spineSway * 0.5f;
        nodeTransform = nodeTransform * glm::rotate(glm::mat4(1.0f), headNod, glm::vec3(0.0f, 0.0f, 1.0f)) *
            glm::rotate(glm::mat4(1.0f), headCounterSway, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    // [尾巴]
    float tailSway = -sin(cycleRad) * glm::radians(30.0f);
    if (nodeName == "Bip01_Tail") { nodeTransform = nodeTransform * glm::rotate(glm::mat4(1.0f), tailSway, glm::vec3(0.0f, 1.0f, 0.0f)); }
    if (nodeName == "Bip01_Tail1") { nodeTransform = nodeTransform * glm::rotate(glm::mat4(1.0f), tailSway * 1.2f, glm::vec3(0.0f, 1.0f, 0.0f)); }
    if (nodeName == "Bip01_Tail2") { nodeTransform = nodeTransform * glm::rotate(glm::mat4(1.0f), tailSway * 1.4f, glm::vec3(0.0f, 1.0f, 0.0f)); }
    if (nodeName == "Bip01_Tail3" || nodeName == "Bip01_Tail4") {
        nodeTransform = nodeTransform * glm::rotate(glm::mat4(1.0f), tailSway * 1.6f, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    // [手臂]
    float armSwing = -sin(cycleRad) * glm::radians(10.0f);
    if (nodeName == "Bip01_R_UpperArm") {
        nodeTransform = nodeTransform * glm::rotate(glm::mat4(1.0f), armSwing, glm::vec3(0.0f, 0.0f, 1.0f));
    }
    if (nodeName == "Bip01_L_UpperArm") {
        nodeTransform = nodeTransform * glm::rotate(glm::mat4(1.0f), -armSwing, glm::vec3(0.0f, 0.0f, 1.0f));
    }

    // --- 腿部变换 (保持不变) ---
    float maxForward = glm::radians(35.0f);
    float maxBackward = glm::radians(-12.0f);
    float toeKnuckleBendStance = glm::radians(30.0f);
    float toeKnuckleBendSwing = glm::radians(-35.0f);

    auto applyLegAnimation = [&](float phase, const std::string& thigh, const std::string& calf, const std::string& foot, const std::string& toe_base, const std::string& toe_knuckle) {
        float thighSwing = 0, calfBend = 0, ankleBend = 0, toeBaseBend = 0, toeKnuckleBend = 0;

        if (phase < 0.5f) { // Stance
            float t = phase / 0.5f;
            thighSwing = lerp(maxForward, maxBackward, t);
            calfBend = sin(t * (float)M_PI) * glm::radians(-5.0f);
            float pushOff = pow(std::max(0.0f, (t - 0.5f) * 2.0f), 0.5f);
            ankleBend = pushOff * glm::radians(-15.0f);
            toeBaseBend = pushOff * glm::radians(35.0f);
            toeKnuckleBend = pushOff * toeKnuckleBendStance;
        }
        else { // Swing
            float t = (phase - 0.5f) / 0.5f;
            thighSwing = lerp(maxBackward, maxForward, t);
            calfBend = sin(t * (float)M_PI) * glm::radians(-70.0f);
            ankleBend = sin(t * (float)M_PI) * glm::radians(50.0f);
            toeBaseBend = sin(t * (float)M_PI) * glm::radians(-40.0f);
            toeKnuckleBend = sin(t * (float)M_PI) * toeKnuckleBendSwing;
        }

        if (nodeName == thigh) nodeTransform = nodeTransform * glm::rotate(glm::mat4(1.0f), thighSwing, glm::vec3(0.0f, 0.0f, 1.0f));
        if (nodeName == calf) nodeTransform = nodeTransform * glm::rotate(glm::mat4(1.0f), calfBend, glm::vec3(0.0f, 0.0f, 1.0f));
        if (nodeName == foot) nodeTransform = nodeTransform * glm::rotate(glm::mat4(1.0f), ankleBend, glm::vec3(0.0f, 0.0f, 1.0f));
        if (nodeName == toe_base) nodeTransform = nodeTransform * glm::rotate(glm::mat4(1.0f), toeBaseBend, glm::vec3(0.0f, 0.0f, 1.0f));
        if (nodeName == toe_knuckle) nodeTransform = nodeTransform * glm::rotate(glm::mat4(1.0f), toeKnuckleBend, glm::vec3(0.0f, 0.0f, 1.0f));
    };

    if (nodeName == "Bip01_R_Thigh") applyLegAnimation(rightPhase, "Bip01_R_Thigh", "", "", "", "");
    if (nodeName == "Bip01_R_Calf") applyLegAnimation(rightPhase, "", "Bip01_R_Calf", "", "", "");
    if (nodeName == "Bip01_R_Foot") applyLegAnimation(rightPhase, "", "", "Bip01_R_Foot", "", "");
    if (nodeName == "Bip01_R_Toe0") applyLegAnimation(rightPhase, "", "", "", "Bip01_R_Toe0", "");
    if (nodeName == "Bip01_R_Toe01") applyLegAnimation(rightPhase, "", "", "", "", "Bip01_R_Toe01");
    if (nodeName == "Bip01_R_Toe1") applyLegAnimation(rightPhase, "", "", "", "Bip01_R_Toe1", "");
    if (nodeName == "Bip01_R_Toe11") applyLegAnimation(rightPhase, "", "", "", "", "Bip01_R_Toe11");
    if (nodeName == "Bip01_R_Toe2") applyLegAnimation(rightPhase, "", "", "", "Bip01_R_Toe2", "");
    if (nodeName == "Bip01_R_Toe21") applyLegAnimation(rightPhase, "", "", "", "", "Bip01_R_Toe21");

    if (nodeName == "Bip01_L_Thigh") applyLegAnimation(leftPhase, "Bip01_L_Thigh", "", "", "", "");
    if (nodeName == "Bip01_L_Calf") applyLegAnimation(leftPhase, "", "Bip01_L_Calf", "", "", "");
    if (nodeName == "Bip01_L_Foot") applyLegAnimation(leftPhase, "", "", "Bip01_L_Foot", "", "");
    if (nodeName == "Bip01_L_Toe0") applyLegAnimation(leftPhase, "", "", "", "Bip01_L_Toe0", "");
    if (nodeName == "Bip01_L_Toe01") applyLegAnimation(leftPhase, "", "", "", "", "Bip01_L_Toe01");
    if (nodeName == "Bip01_L_Toe1") applyLegAnimation(leftPhase, "", "", "", "Bip01_L_Toe1", "");
    if (nodeName == "Bip01_L_Toe11") applyLegAnimation(leftPhase, "", "", "", "", "Bip01_L_Toe11");
    if (nodeName == "Bip01_L_Toe2") applyLegAnimation(leftPhase, "", "", "", "Bip01_L_Toe2", "");
    if (nodeName == "Bip01_L_Toe21") applyLegAnimation(leftPhase, "", "", "", "", "Bip01_L_Toe21");


    // --- 3. 计算最终矩阵 (层级动画核心修改) ---
    glm::mat4 globalTransformation = parentTransform * nodeTransform;

    auto boneInfoMap = m_CurrentAnimation->GetBoneIDMap();
    if (boneInfoMap.find(nodeName) != boneInfoMap.end())
    {
        int index = boneInfoMap.at(nodeName).id;
        // [修改]：不再乘以 Offset Matrix。
        // 在层级动画中，网格直接跟随节点移动，不需要反向绑定。
        // m_FinalBoneMatrices[index] = globalTransformation * offset; (旧)

        m_FinalBoneMatrices[index] = globalTransformation; // (新)
    }

    for (int i = 0; i < node->childrenCount; i++)
        CalculateBoneTransform(&node->children[i], globalTransformation);
}

std::vector<glm::mat4>& Animator::GetFinalBoneMatrices()
{
    return m_FinalBoneMatrices;
}