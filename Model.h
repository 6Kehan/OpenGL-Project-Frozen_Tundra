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

// [新增] 记录节点和网格的对应关系
struct NodeMeshMap {
    std::string nodeName;
    std::vector<unsigned int> meshIndices; // 该节点下挂载的 Mesh 在 meshes 数组中的索引
};

class Model
{
public:
    std::map<std::string, BoneInfo> m_BoneInfoMap;
    int m_BoneCounter = 0;

    std::vector<Texture> textures_loaded;
    std::vector<Mesh>    meshes;
    std::string directory;

    // [新增] 存储节点到网格索引的映射
    std::vector<NodeMeshMap> nodeMeshMap;

    Model();
    Model(std::string const& path);

    void LoadFromScene(const aiScene* scene, const std::string& modelDirectory);

    void Draw(Shader& shader);

    // [新增] 层级绘制函数
    void DrawHierarchical(Shader& shader, const std::vector<glm::mat4>& boneMatrices, const std::map<std::string, BoneInfo>& boneInfoMap, glm::mat4 baseModelMatrix);

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