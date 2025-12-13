#ifndef MESH_H
#define MESH_H

#include <GL/glew.h>
#include "glm/glm.hpp"
#include <string>
#include <vector>
#include "Shader.h"
#include "Bone.h" // 包含我们刚创建的新文件

// =========================================================
// ↓↓↓ 'Vertex' 结构体已彻底更新 ↓↓↓
// =========================================================
struct Vertex {
    // 之前的数据
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
    glm::vec3 Tangent;
    glm::vec3 Bitangent;

    // --- 新增：骨骼动画数据 ---
    int m_BoneIDs[MAX_BONE_INFLUENCE];
    float m_Weights[MAX_BONE_INFLUENCE];
};
// =========================================================

struct Texture {
    unsigned int id;
    std::string type;
    std::string path;
};

class Mesh {
public:
    std::vector<Vertex>       vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture>      textures;
    unsigned int VAO;

    Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures);
    void Draw(Shader& shader);

private:
    unsigned int VBO, EBO;
    void setupMesh();
};
#endif