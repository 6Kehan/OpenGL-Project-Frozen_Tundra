#include "Model.h"
#include <sstream> // 用于字符串到 int 的转换

// 新增：默认构造函数的实现
Model::Model() : m_BoneCounter(0) {}

// 修改：确保 'path' 构造函数也初始化了 m_BoneCounter
Model::Model(std::string const& path) : m_BoneCounter(0)
{
    loadModel(path);
}
// (它只是 loadModel 的一部分，但使用一个外部的 scene)
void Model::LoadFromScene(const aiScene* scene, const std::string& modelDirectory)
{
    this->directory = modelDirectory;
    processNode(scene->mRootNode, scene);
}
void Model::Draw(Shader& shader)
{
    for (unsigned int i = 0; i < meshes.size(); i++)
        meshes[i].Draw(shader);
}

void Model::loadModel(std::string const& path)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cout << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
        return;
    }
    directory = path.substr(0, path.find_last_of('/'));

    processNode(scene->mRootNode, scene);
}

// =========================================================
// ↓↓↓ 已更新：传递 const aiScene* scene ↓↓↓
// =========================================================
void Model::processNode(aiNode* node, const aiScene* scene)
{
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        meshes.push_back(processMesh(mesh, scene));
    }
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processNode(node->mChildren[i], scene);
    }
}
// =========================================================

Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene)
{
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;

    // 1. 处理顶点
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;
        SetVertexBoneDataToDefault(vertex);
        vertex.Position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
        if (mesh->HasNormals())
            vertex.Normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
        if (mesh->mTextureCoords[0])
            vertex.TexCoords = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
        else
            vertex.TexCoords = glm::vec2(0.0f, 0.0f);
        if (mesh->HasTangentsAndBitangents())
        {
            vertex.Tangent = glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
            vertex.Bitangent = glm::vec3(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);
        }
        vertices.push_back(vertex);
    }

    // 2. 提取骨骼权重
    ExtractBoneWeightForVertices(vertices, mesh, scene);

    // 3. 处理索引
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
            indices.push_back(face.mIndices[j]);
    }

    // =========================================================
    // ↓↓↓ 4. 处理材质 (已更新：传递 scene) ↓↓↓
    // =========================================================
    aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

    std::vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse", scene);
    textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());

    std::vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular", scene);
    textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());

    std::vector<Texture> normalMaps = loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal", scene);
    textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());

    std::vector<Texture> normalMaps_assimp = loadMaterialTextures(material, aiTextureType_NORMALS, "texture_normal", scene);
    textures.insert(textures.end(), normalMaps_assimp.begin(), normalMaps_assimp.end());
    // =========================================================

    return Mesh(vertices, indices, textures);
}


void Model::SetVertexBoneDataToDefault(Vertex& vertex)
{
    for (int i = 0; i < MAX_BONE_INFLUENCE; i++)
    {
        vertex.m_BoneIDs[i] = -1;
        vertex.m_Weights[i] = 0.0f;
    }
}

void Model::ExtractBoneWeightForVertices(std::vector<Vertex>& vertices, aiMesh* mesh, const aiScene* scene)
{
    for (int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
    {
        int boneID = -1;
        std::string boneName = mesh->mBones[boneIndex]->mName.C_Str();

        if (m_BoneInfoMap.find(boneName) == m_BoneInfoMap.end())
        {
            BoneInfo newBoneInfo;
            newBoneInfo.id = m_BoneCounter;
            newBoneInfo.offset = AssimpToGLM_Matrix(mesh->mBones[boneIndex]->mOffsetMatrix);
            m_BoneInfoMap[boneName] = newBoneInfo;
            boneID = m_BoneCounter;
            m_BoneCounter++;
        }
        else
        {
            boneID = m_BoneInfoMap[boneName].id;
        }

        if (boneID == -1)
            continue;

        aiVertexWeight* weights = mesh->mBones[boneIndex]->mWeights;
        int numWeights = mesh->mBones[boneIndex]->mNumWeights;

        for (int weightIndex = 0; weightIndex < numWeights; ++weightIndex)
        {
            int vertexId = weights[weightIndex].mVertexId;
            float weight = weights[weightIndex].mWeight;

            if (vertexId >= vertices.size())
                continue;

            for (int i = 0; i < MAX_BONE_INFLUENCE; ++i)
            {
                if (vertices[vertexId].m_BoneIDs[i] < 0)
                {
                    vertices[vertexId].m_Weights[i] = weight;
                    vertices[vertexId].m_BoneIDs[i] = boneID;
                    break;
                }
            }
        }
    }
}

// =========================================================
// ↓↓↓ 'loadMaterialTextures' 已彻底更新 ↓↓↓
// =========================================================
std::vector<Texture> Model::loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName, const aiScene* scene)
{
    std::vector<Texture> textures;
    for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
    {
        aiString str;
        mat->GetTexture(type, i, &str);
        bool skip = false;

        // 检查纹理是否已经被加载过
        for (unsigned int j = 0; j < textures_loaded.size(); j++)
        {
            if (std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0)
            {
                textures.push_back(textures_loaded[j]);
                skip = true;
                break;
            }
        }

        if (!skip)
        {
            // 如果纹理未被加载过，加载它
            Texture texture;

            // --- 这是关键的修复 ---
            // 检查贴图路径是否以 '*' 开头
            if (str.C_Str()[0] == '*')
            {
                // 是嵌入式贴图
                std::cout << "Loading embedded texture: " << str.C_Str() << std::endl;

                // 从 '*' 后面解析出索引 ID
                std::string indexStr = std::string(str.C_Str()).substr(1);
                std::stringstream ss(indexStr);
                int textureIndex;
                ss >> textureIndex;

                // 从 aiScene 中获取嵌入式贴图数据
                const aiTexture* embeddedTex = scene->mTextures[textureIndex];

                // 调用新的加载函数
                texture.id = TextureFromEmbedded(embeddedTex);
            }
            else
            {
                // 是外部文件贴图
                // 调用旧的加载函数
                texture.id = TextureFromFile(str.C_Str(), this->directory);
            }
            // --- 修复结束 ---

            texture.type = typeName;
            texture.path = str.C_Str(); // 我们仍然使用路径/ID作为 "path" 来防止重复加载
            textures.push_back(texture);
            textures_loaded.push_back(texture);
        }
    }
    return textures;
}
// =========================================================