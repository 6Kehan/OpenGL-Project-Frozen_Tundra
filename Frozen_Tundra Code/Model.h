#ifndef MODEL_H
#define MODEL_H

#include <GL/glew.h>
#include "glm/glm.hpp"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Mesh.h"
#include "Shader.h"
#include "Bone.h" 

#include <string>
#include <vector>
#include <iostream>
#include <algorithm> 
#include <map>

// 声明两个纹理加载函数
unsigned int TextureFromFile(const char* path, const std::string& directory);
unsigned int TextureFromEmbedded(const aiTexture* tex);

class Model
{
public:
    std::map<std::string, BoneInfo> m_BoneInfoMap;
    int m_BoneCounter = 0;

    std::vector<Texture> textures_loaded;
    std::vector<Mesh>    meshes;
    std::string directory;

    // --- 修改开始 ---
    Model(); // <-- 新增：默认构造函数
    Model(std::string const& path); // <-- 保留：用于加载地形

    // <-- 新增：从一个已加载的 scene 中初始化模型
    void LoadFromScene(const aiScene* scene, const std::string& modelDirectory);
    // --- 修改结束 ---

    void Draw(Shader& shader);

    auto& GetBoneInfoMap() { return m_BoneInfoMap; }
    int& GetBoneCount() { return m_BoneCounter; }

private:
    void loadModel(std::string const& path);

    void processNode(aiNode* node, const aiScene* scene);
    Mesh processMesh(aiMesh* mesh, const aiScene* scene);
    std::vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName, const aiScene* scene);

    void SetVertexBoneDataToDefault(Vertex& vertex);
    void ExtractBoneWeightForVertices(std::vector<Vertex>& vertices, aiMesh* mesh, const aiScene* scene);
};
#endif