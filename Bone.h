#ifndef BONE_H
#define BONE_H

#include <string>
#include <vector>
#include <map>
#include <assimp/scene.h>
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp" // 包含 glm::value_ptr
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/quaternion.hpp"
#include <cstring> // 包含 memcpy

// =========================================================
// ↓↓↓ 1. AssimpToGLM_Matrix 函数 (已修正) ↓↓↓
// (执行正确的“转置”操作，修复模型扭曲)
// =========================================================
inline glm::mat4 AssimpToGLM_Matrix(const aiMatrix4x4& from)
{
    glm::mat4 to;

    // Assimp (行主序) 的第 1 行 -> GLM (列主序) 的第 1 列
    to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
    // Assimp (行主序) 的第 2 行 -> GLM (列主序) 的第 2 列
    to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
    // Assimp (行主序) 的第 3 行 -> GLM (列主序) 的第 3 列
    to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
    // Assimp (行主序) 的第 4 行 -> GLM (列主序) 的第 4 列
    to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;

    return to;
}
// =========================================================

// (这个函数是正确的)
inline glm::vec3 AssimpToGLM_Vec3(const aiVector3D& vec)
{
    return glm::vec3(vec.x, vec.y, vec.z);
}

// (这个函数是正确的)
inline glm::quat AssimpToGLM_Quat(const aiQuaternion& pQuat)
{
    // GLM 构造函数是 (w, x, y, z)
    return glm::quat(pQuat.w, pQuat.x, pQuat.y, pQuat.z);
}

// -------------------------------------------------
// 骨骼动画所需的数据结构
// -------------------------------------------------
#define MAX_BONE_INFLUENCE 4

struct BoneInfo
{
    int id;
    glm::mat4 offset;
};

// -------------------------------------------------
// 关键帧数据结构
// -------------------------------------------------
struct PositionKey
{
    glm::vec3 position;
    float timeStamp;
};

struct RotationKey
{
    glm::quat orientation;
    float timeStamp;
};

struct ScaleKey
{
    glm::vec3 scale;
    float timeStamp;
};

// -------------------------------------------------
// Bone 类：处理单根骨骼的动画
// -------------------------------------------------
class Bone
{
private:
    std::vector<PositionKey> m_Positions;
    std::vector<RotationKey> m_Rotations;
    std::vector<ScaleKey> m_Scales;
    int m_NumPositions;
    int m_NumRotations;
    int m_NumScalings;

    glm::mat4 m_LocalTransform;
    std::string m_Name;
    int m_ID;

public:
    Bone(const std::string& name, int ID, const aiNodeAnim* channel)
        : m_Name(name), m_ID(ID), m_LocalTransform(1.0f)
    {
        m_NumPositions = channel->mNumPositionKeys;
        m_Positions.reserve(m_NumPositions); // 优化：预分配内存
        for (int i = 0; i < m_NumPositions; ++i)
        {
            PositionKey key;
            key.position = AssimpToGLM_Vec3(channel->mPositionKeys[i].mValue);
            key.timeStamp = (float)channel->mPositionKeys[i].mTime;
            m_Positions.push_back(key);
        }

        m_NumRotations = channel->mNumRotationKeys;
        m_Rotations.reserve(m_NumRotations); // 优化：预分配内存
        for (int i = 0; i < m_NumRotations; ++i)
        {
            RotationKey key;
            key.orientation = AssimpToGLM_Quat(channel->mRotationKeys[i].mValue);
            key.timeStamp = (float)channel->mRotationKeys[i].mTime;
            m_Rotations.push_back(key);
        }

        m_NumScalings = channel->mNumScalingKeys;
        m_Scales.reserve(m_NumScalings); // 优化：预分配内存
        for (int i = 0; i < m_NumScalings; ++i)
        {
            ScaleKey key;
            key.scale = AssimpToGLM_Vec3(channel->mScalingKeys[i].mValue);
            key.timeStamp = (float)channel->mScalingKeys[i].mTime;
            m_Scales.push_back(key);
        }
    }

    void Update(float animationTime)
    {
        glm::mat4 translation = InterpolatePosition(animationTime);
        glm::mat4 rotation = InterpolateRotation(animationTime);
        glm::mat4 scale = InterpolateScaling(animationTime);
        m_LocalTransform = translation * rotation * scale;
    }

    glm::mat4 GetLocalTransform() { return m_LocalTransform; }
    std::string GetBoneName() const { return m_Name; }
    int GetBoneID() { return m_ID; }

private:
    float GetScaleFactor(float lastTimeStamp, float nextTimeStamp, float animationTime)
    {
        float scaleFactor = 0.0f;
        float midWayLength = animationTime - lastTimeStamp;
        float framesDiff = nextTimeStamp - lastTimeStamp;
        // 避免除以零
        if (framesDiff == 0.0f) return 0.0f;
        scaleFactor = midWayLength / framesDiff;
        return scaleFactor;
    }

    glm::mat4 InterpolatePosition(float animationTime)
    {
        if (m_NumPositions == 1)
            return glm::translate(glm::mat4(1.0f), m_Positions[0].position);

        int p0Index = GetPositionIndex(animationTime);
        int p1Index = p0Index + 1;
        // 确保 p1Index 不会越界 (虽然 GetPositionIndex 应该已经处理了)
        if (p1Index >= m_NumPositions) p1Index = m_NumPositions - 1;

        float scaleFactor = GetScaleFactor(m_Positions[p0Index].timeStamp,
            m_Positions[p1Index].timeStamp, animationTime);
        glm::vec3 finalPosition = glm::mix(m_Positions[p0Index].position,
            m_Positions[p1Index].position, scaleFactor);
        return glm::translate(glm::mat4(1.0f), finalPosition);
    }

    glm::mat4 InterpolateRotation(float animationTime)
    {
        if (m_NumRotations == 1)
            return glm::toMat4(glm::normalize(m_Rotations[0].orientation));

        int p0Index = GetRotationIndex(animationTime);
        int p1Index = p0Index + 1;
        if (p1Index >= m_NumRotations) p1Index = m_NumRotations - 1;

        float scaleFactor = GetScaleFactor(m_Rotations[p0Index].timeStamp,
            m_Rotations[p1Index].timeStamp, animationTime);
        glm::quat finalRotation = glm::slerp(m_Rotations[p0Index].orientation,
            m_Rotations[p1Index].orientation, scaleFactor);
        finalRotation = glm::normalize(finalRotation);
        return glm::toMat4(finalRotation);
    }

    // =========================================================
    // ↓↓↓ 拼写错误 (m_ScalES) 已修复 ↓↓↓
    // =========================================================
    glm::mat4 InterpolateScaling(float animationTime)
    {
        if (m_NumScalings == 1)
            return glm::scale(glm::mat4(1.0f), m_Scales[0].scale);

        int p0Index = GetScaleIndex(animationTime);
        int p1Index = p0Index + 1;
        if (p1Index >= m_NumScalings) p1Index = m_NumScalings - 1;

        // 修复: m_ScalES -> m_Scales
        float scaleFactor = GetScaleFactor(m_Scales[p0Index].timeStamp,
            m_Scales[p1Index].timeStamp, animationTime);
        glm::vec3 finalScale = glm::mix(m_Scales[p0Index].scale,
            m_Scales[p1Index].scale, scaleFactor);
        return glm::scale(glm::mat4(1.0f), finalScale);
    }
    // =========================================================

    // (这些查找函数可以保持原样，但返回最后一个索引更安全)
    int GetPositionIndex(float animationTime)
    {
        for (int index = 0; index < m_NumPositions - 1; ++index)
            if (animationTime < m_Positions[index + 1].timeStamp)
                return index;
        // 如果循环结束 (animationTime > 最后一个 key)
        // 返回倒数第二个索引，以便插值到最后一个
        return m_NumPositions - 2;
    }
    int GetRotationIndex(float animationTime)
    {
        for (int index = 0; index < m_NumRotations - 1; ++index)
            if (animationTime < m_Rotations[index + 1].timeStamp)
                return index;
        return m_NumRotations - 2;
    }
    int GetScaleIndex(float animationTime)
    {
        for (int index = 0; index < m_NumScalings - 1; ++index)
            if (animationTime < m_Scales[index + 1].timeStamp)
                return index;
        return m_NumScalings - 2;
    }
};

// 描述 Assimp 节点层级的数据结构
struct AssimpNodeData
{
    glm::mat4 transformation;
    std::string name;
    int childrenCount;
    std::vector<AssimpNodeData> children;

    // 添加默认构造函数以方便 std::vector
    AssimpNodeData() : childrenCount(0), transformation(1.0f) {}
};

// =========================================================
// ↓↓↓ 修复：添加了缺失的 #endif ↓↓↓
// =========================================================
#endif