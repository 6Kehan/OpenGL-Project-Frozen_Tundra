#include "Model.h"
#include <sstream> 

Model::Model() : m_BoneCounter(0) {}

Model::Model(std::string const& path) : m_BoneCounter(0)
{
    loadModel(path);
}

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

// [新增] 实现层级绘制
void Model::DrawHierarchical(Shader& shader, const std::vector<glm::mat4>& boneMatrices, const std::map<std::string, BoneInfo>& boneInfoMap, glm::mat4 baseModelMatrix)
{
    // 遍历所有有网格的节点
    for (const auto& entry : nodeMeshMap) {
        glm::mat4 nodeTransform = glm::mat4(1.0f);

        // 查找该节点在 Animator 中的变换矩阵
        if (boneInfoMap.find(entry.nodeName) != boneInfoMap.end()) {
            int id = boneInfoMap.at(entry.nodeName).id;
            if (id >= 0 && id < boneMatrices.size()) {
                nodeTransform = boneMatrices[id];
            }
        }

        // 计算最终的世界变换：基础模型位置 * 节点相对位置
        glm::mat4 finalModelMatrix = baseModelMatrix * nodeTransform;

        // 设置 Shader 的 model uniform
        shader.setMat4("model", finalModelMatrix);

        // 绘制该节点下的所有网格
        for (unsigned int meshIdx : entry.meshIndices) {
            meshes[meshIdx].Draw(shader);
        }
    }
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
// [修改] processNode: 建立 节点->网格 映射并注册 BoneID
// =========================================================
void Model::processNode(aiNode* node, const aiScene* scene)
{
    if (node->mNumMeshes > 0) {
        std::cout << "[FOUND MESH] Node: " << node->mName.C_Str()
            << " has " << node->mNumMeshes << " meshes." << std::endl;
    }
    else {
        // Uncomment if you want to see empty nodes
        // std::cout << "Node: " << node->mName.C_Str() << " (Bone/Empty)" << std::endl;
    }
    //std::cout << "Node Name: " << node->mName.C_Str() << std::endl;
    // 1. 记录当前节点包含的网格
    NodeMeshMap nodeEntry;
    nodeEntry.nodeName = node->mName.C_Str();
    bool hasMeshes = false;

    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        meshes.push_back(processMesh(mesh, scene));

        // 记录刚刚添加的这个 mesh 的索引
        nodeEntry.meshIndices.push_back((unsigned int)meshes.size() - 1);
        hasMeshes = true;
    }

    if (hasMeshes) {
        nodeMeshMap.push_back(nodeEntry);
    }

    // 2. [关键] 即使该节点没有 Mesh (如纯骨骼节点)，我们也需要给它分配一个 ID，
    // 这样 Animator 遍历时才能计算它的 Transform 并存入 m_FinalBoneMatrices。
    // 如果该节点在 m_BoneInfoMap 中不存在，则添加它。
    // (注意：对于层级动画，Offset Matrix 不重要，设为 Identity 即可)
    if (m_BoneInfoMap.find(node->mName.C_Str()) == m_BoneInfoMap.end()) {
        BoneInfo info;
        info.id = m_BoneCounter++;
        info.offset = glm::mat4(1.0f);
        m_BoneInfoMap[node->mName.C_Str()] = info;
    }

    // 3. 递归子节点
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processNode(node->mChildren[i], scene);
    }
}

Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene)
{
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;

    // 1. 处理顶点
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;
        SetVertexBoneDataToDefault(vertex); // 保留初始化，防止垃圾数据
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

    // [修改]：不再提取骨骼权重 (ExtractBoneWeightForVertices)。
    // 层级动画不使用顶点权重，每个 Mesh 整体移动。
    // ExtractBoneWeightForVertices(vertices, mesh, scene); 

    // 3. 处理索引
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
            indices.push_back(face.mIndices[j]);
    }

    // 4. 处理材质
    aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

    std::vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse", scene);
    textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());

    std::vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular", scene);
    textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());

    std::vector<Texture> normalMaps = loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal", scene);
    textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());

    std::vector<Texture> normalMaps_assimp = loadMaterialTextures(material, aiTextureType_NORMALS, "texture_normal", scene);
    textures.insert(textures.end(), normalMaps_assimp.begin(), normalMaps_assimp.end());

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

// ExtractBoneWeightForVertices 已被上面注释掉，这里可以保留空实现或直接保留原代码，因为它不再被调用
void Model::ExtractBoneWeightForVertices(std::vector<Vertex>& vertices, aiMesh* mesh, const aiScene* scene)
{
    // ... 原有逻辑 (不再使用) ...
}

std::vector<Texture> Model::loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName, const aiScene* scene)
{
    std::vector<Texture> textures;
    for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
    {
        aiString str;
        mat->GetTexture(type, i, &str);
        bool skip = false;

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
            Texture texture;
            if (str.C_Str()[0] == '*')
            {
                std::string indexStr = std::string(str.C_Str()).substr(1);
                std::stringstream ss(indexStr);
                int textureIndex;
                ss >> textureIndex;
                const aiTexture* embeddedTex = scene->mTextures[textureIndex];
                texture.id = TextureFromEmbedded(embeddedTex);
            }
            else
            {
                texture.id = TextureFromFile(str.C_Str(), this->directory);
            }
            texture.type = typeName;
            texture.path = str.C_Str();
            textures.push_back(texture);
            textures_loaded.push_back(texture);
        }
    }
    return textures;
}