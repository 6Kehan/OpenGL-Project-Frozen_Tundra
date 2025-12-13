#ifndef ANIMATION_H
#define ANIMATION_H

#include <vector>
#include <map>
#include <string>
#include <assimp/scene.h>
#include "Bone.h"
#include "Model.h"

// 这是一个辅助结构体，用于存储一个骨骼的完整动画通道
struct BoneNode
{
    std::vector<PositionKey> m_Positions;
    std::vector<RotationKey> m_Rotations;
    std::vector<ScaleKey> m_Scales;

};

// Animation 类
class Animation
{
public:
    Animation() = default;

    // 构造函数：从 Assimp 场景中加载动画数据
    Animation(const aiScene* scene, Model* model)
    {
        // 确保 Assimp 场景中至少有一个动画
        if (!scene->HasAnimations()) {
            std::cout << "Error: No animations found in scene!" << std::endl;
            return;
        }

        // 我们假设第一个动画就是我们要的“行走”动画
        // (在更高级的引擎中，你会按名称 "walk" 来搜索)
        auto animation = scene->mAnimations[0];
        m_Duration = (float)animation->mDuration;
        m_TicksPerSecond = (float)animation->mTicksPerSecond;
        if (m_TicksPerSecond == 0) m_TicksPerSecond = 25.0f; // 默认值

        ReadHierarchyData(m_RootNode, scene->mRootNode);
        ReadMissingBones(animation, *model);
    }

    ~Animation() {}

    // 根据名称查找骨骼
    Bone* FindBone(const std::string& name)
    {
        auto iter = std::find_if(m_Bones.begin(), m_Bones.end(),
            [&](const Bone& Bone)
            {
                return Bone.GetBoneName() == name;
            }
        );
        if (iter == m_Bones.end()) return nullptr;
        else return &(*iter);
    }

    // 获取动画数据
    float GetTicksPerSecond() { return m_TicksPerSecond; }
    float GetDuration() { return m_Duration; }
    const AssimpNodeData& GetRootNode() { return m_RootNode; }
    const std::map<std::string, BoneInfo>& GetBoneIDMap() { return m_BoneInfoMap; }

private:
    // 递归读取骨骼的层级结构
    void ReadHierarchyData(AssimpNodeData& dest, const aiNode* src)
    {
        dest.name = src->mName.data;
        dest.transformation = AssimpToGLM_Matrix(src->mTransformation);
        dest.childrenCount = src->mNumChildren;

        for (unsigned int i = 0; i < src->mNumChildren; i++)
        {
            AssimpNodeData newData;
            ReadHierarchyData(newData, src->mChildren[i]);
            dest.children.push_back(newData);
        }
    }

    void ReadMissingBones(const aiAnimation* animation, Model& model)
    {
        int size = animation->mNumChannels;

        // =========================================================
        // ↓↓↓ 这是关键的逻辑修复 ↓↓↓
        // =========================================================

        // 1. 从 Model 复制一份“纯净”的 BoneInfoMap。
        //    这个 Map 只包含真正变形的骨骼和它们正确的 Offset 矩阵。
        m_BoneInfoMap = model.GetBoneInfoMap();

        // 2. [删除] 我们不再需要修改 Model 的 boneCount
        // int& boneCount = model.GetBoneCount(); // <-- 已删除

        // 3. 遍历所有的“动画通道”(mChannels)
        //    我们需要 m_Bones 列表包含 *所有* 正在动的骨骼，
        //    以便 Animator 可以计算完整的骨骼层级。
        for (int i = 0; i < size; i++)
        {
            auto channel = animation->mChannels[i];
            std::string boneName = channel->mNodeName.data;

            // [旧的错误代码 - 已删除]
            // if (boneInfoMap.find(boneName) == boneInfoMap.end())
            // { ... }

            // [新的正确逻辑]
            // 在 m_Bones 中为 *每个* 通道添加一个 Bone 对象。

            int boneID = -1; // 默认为 -1 (表示这个骨骼只用于动画层级)

            if (m_BoneInfoMap.find(boneName) != m_BoneInfoMap.end())
            {
                // 这个骨骼既在动画中，也在变形 map 中，
                // 所以我们使用它在 Model 中已经注册的 ID。
                boneID = m_BoneInfoMap.at(boneName).id;
            }

            m_Bones.push_back(Bone(channel->mNodeName.data, boneID, channel));
        }

        // [删除]
        // m_BoneInfoMap = boneInfoMap; // <-- 已删除 (我们已在函数开头设置了它)
        // =========================================================
    }

    float m_Duration;
    float m_TicksPerSecond;
    std::vector<Bone> m_Bones;
    AssimpNodeData m_RootNode;
    std::map<std::string, BoneInfo> m_BoneInfoMap;
};
#endif