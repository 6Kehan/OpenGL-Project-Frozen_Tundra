#include <GL/glew.h>
#include <GL/freeglut.h>
#include <iostream>
#include <string>
#include <algorithm>
#include <vector>
#include <ctime> 

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" 

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include "Shader.h"
#include "Camera.h"
#include "Model.h" 
#include "Bone.h"
#include "Animation.h"
#include "Animator.h"

// 窗口与相机
int screenWidth = 1280;
int screenHeight = 720;
Camera camera(glm::vec3(-150.0f, 0.0f, 150.0f));
float lastX = screenWidth / 2.0f;
float lastY = screenHeight / 2.0f;
bool firstMouse = true;

// 时间与输入
float deltaTime = 0.0f;
float lastFrame = 0.0f;
float modelScale = 1.0f;
bool keys[256];

// 全局对象
Shader* ourShader = nullptr;
Shader* depthShader = nullptr;
Shader* snowShader = nullptr;
Model* terrainModel = nullptr;
Model* animatedModel = nullptr;
Model* skyboxModel = nullptr;
Animation* animation = nullptr;
Animator* animator = nullptr;

// 场景参数
glm::vec3 lightPos(0.0f, 50.0f, 20.0f);
glm::vec3 dinoPos(-150.0f, -20.0f, -10.0f); // 恐龙高度 -20
glm::vec3 dinoForward(1.0f, 0.01f, 1.0f);
float dinoSpeed = 5.0f; // 略微加快速度，方便测试
float dinoRotationAngle = -60.0f; // [新增] 恐龙朝向变量

// 阴影参数
unsigned int depthMapFBO;
unsigned int depthMap;
const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;

// 备用纹理
unsigned int whiteTextureID;

// 下雪粒子
struct Snowflake {
    glm::vec3 position;
    glm::vec3 velocity;
    float size;
};
const int SNOW_COUNT = 6000;
std::vector<Snowflake> snowflakes;
unsigned int snowVAO, snowVBO;
unsigned int snowTextureID;
const float SNOW_RANGE = 120.0f;

// ---------------------------------------------------------
// Shaders
// ---------------------------------------------------------
const char* V_SHADER_SOURCE = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;
layout (location = 5) in ivec4 aBoneIDs; 
layout (location = 6) in vec4 aWeights; 
out vec2 TexCoords;
out vec3 FragPos;
out mat3 TBN;
out vec4 FragPosLightSpace;
uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform mat4 lightSpaceMatrix;
const int MAX_BONES = 100;
uniform mat4 finalBonesMatrices[MAX_BONES];
void main()
{
    mat4 totalBoneTransform = mat4(0.0);
    float weightSum = aWeights.x + aWeights.y + aWeights.z + aWeights.w;
    if(weightSum > 0.0) {
        for(int i = 0; i < 4; i++) {
            if(aBoneIDs[i] == -1) continue;
            if(aBoneIDs[i] >= MAX_BONES) { totalBoneTransform = mat4(1.0); break; }
            totalBoneTransform += finalBonesMatrices[aBoneIDs[i]] * aWeights[i];
        }
    } else { totalBoneTransform = mat4(1.0); }
    if (totalBoneTransform[0][0] == 0.0) totalBoneTransform = mat4(1.0);

    vec4 worldPos = model * totalBoneTransform * vec4(aPos, 1.0);
    vec3 worldNormal = mat3(model * totalBoneTransform) * aNormal;
    vec3 worldTangent = mat3(model * totalBoneTransform) * aTangent;
    vec3 worldBitangent = mat3(model * totalBoneTransform) * aBitangent;

    gl_Position = projection * view * worldPos;
    FragPos = vec3(worldPos);
    TexCoords = aTexCoords;
    vec3 N = normalize(worldNormal);
    vec3 T = normalize(worldTangent);
    vec3 B = normalize(worldBitangent);
    TBN = mat3(T, B, N);
    FragPosLightSpace = lightSpaceMatrix * worldPos;
}
)glsl";

const char* F_SHADER_SOURCE = R"glsl(
#version 330 core
out vec4 FragColor;
in vec2 TexCoords;
in vec3 FragPos;
in mat3 TBN; 
in vec4 FragPosLightSpace;
uniform sampler2D texture_diffuse1;
uniform sampler2D texture_specular1;
uniform sampler2D texture_normal1;
uniform sampler2D shadowMap; 
uniform vec3 lightPos; 
uniform vec3 lightColor;
uniform vec3 viewPos;
uniform bool enableLighting; 

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if(projCoords.z > 1.0) return 0.0;
    float currentDepth = projCoords.z;
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;
    return shadow;
}

void main()
{    
    vec4 texData = texture(texture_diffuse1, TexCoords);
    vec3 texColor = texData.rgb;
    // 纹理防黑屏保护
    if(texData.a < 0.1) texColor = vec3(0.7, 0.7, 0.7); 

    if(!enableLighting) {
        FragColor = vec4(texColor, 1.0);
        return;
    }

    vec3 normal = texture(texture_normal1, TexCoords).rgb;
    if(length(normal) < 0.1) normal = TBN[2]; 
    else {
        normal = normalize(normal * 2.0 - 1.0);   
        normal = normalize(TBN * normal); 
    }

    float ambientStrength = 0.4; 
    vec3 ambient = ambientStrength * lightColor;
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, normal);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specularColor = vec3(texture(texture_specular1, TexCoords));
    vec3 specular = 0.5 * spec * lightColor * specularColor;
    float shadow = ShadowCalculation(FragPosLightSpace, normal, lightDir);
    vec3 result = (ambient + (1.0 - shadow) * (diffuse + specular)) * texColor;
    FragColor = vec4(result, 1.0);
}
)glsl";

const char* DEPTH_VS_SOURCE = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 5) in ivec4 aBoneIDs;
layout (location = 6) in vec4 aWeights;
uniform mat4 lightSpaceMatrix;
uniform mat4 model;
const int MAX_BONES = 100;
uniform mat4 finalBonesMatrices[MAX_BONES];
void main()
{
    mat4 totalBoneTransform = mat4(0.0);
    float weightSum = aWeights.x + aWeights.y + aWeights.z + aWeights.w;
    if(weightSum > 0.0) {
        for(int i = 0; i < 4; i++) {
            if(aBoneIDs[i] == -1) continue;
            if(aBoneIDs[i] >= MAX_BONES) { totalBoneTransform = mat4(1.0); break; }
            totalBoneTransform += finalBonesMatrices[aBoneIDs[i]] * aWeights[i];
        }
    } else { totalBoneTransform = mat4(1.0); }
    if (totalBoneTransform[0][0] == 0.0) totalBoneTransform = mat4(1.0);
    gl_Position = lightSpaceMatrix * model * totalBoneTransform * vec4(aPos, 1.0);
}
)glsl";

const char* DEPTH_FS_SOURCE = R"glsl(
#version 330 core
void main() {}
)glsl";

const char* SNOW_VS_SOURCE = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in float aSize;
uniform mat4 projection;
uniform mat4 view;
void main()
{
    vec4 viewPos = view * vec4(aPos, 1.0);
    gl_Position = projection * viewPos;
    float dist = length(viewPos.xyz);
    gl_PointSize = aSize * (300.0 / dist); 
}
)glsl";

const char* SNOW_FS_SOURCE = R"glsl(
#version 330 core
out vec4 FragColor;
uniform sampler2D spriteTexture;
void main()
{
    vec4 texColor = texture(spriteTexture, gl_PointCoord);
    if(texColor.a < 0.1) discard;
    FragColor = vec4(1.0, 1.0, 1.0, 0.4) * texColor;
}
)glsl";

// ---------------------------------------------------------
// 函数声明
// ---------------------------------------------------------
void initGL();
void display();
void reshape(int w, int h);
void keyboard(unsigned char key, int x, int y);
void keyboardUp(unsigned char key, int x, int y);
void processInput(float dt);
void mouseMove(int x, int y);
void idle();
unsigned int TextureFromFile(const char* path, const std::string& directory);
unsigned int TextureFromEmbedded(const aiTexture* tex);
void renderScene(Shader& shader);
unsigned int CreateWhiteTexture();
unsigned int CreateSnowTexture();
void InitSnowSystem();
void RenderSnow();

// ---------------------------------------------------------
// Main
// ---------------------------------------------------------
int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitContextVersion(3, 3);
    glutInitContextProfile(GLUT_CORE_PROFILE);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(screenWidth, screenHeight);
    glutCreateWindow("Frozen Tundra - Kehan Liu");

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) return -1;

    srand((unsigned int)time(0));
    initGL();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboardUp);
    glutIdleFunc(idle);
    glutPassiveMotionFunc(mouseMove);
    glutSetCursor(GLUT_CURSOR_NONE);
    glutWarpPointer(screenWidth / 2, screenHeight / 2);

    glutMainLoop();

    if (ourShader) delete ourShader;
    if (depthShader) delete depthShader;
    if (snowShader) delete snowShader;
    if (terrainModel) delete terrainModel;
    if (animatedModel) delete animatedModel;
    if (animation) delete animation;
    if (animator) delete animator;
    if (skyboxModel) delete skyboxModel;

    return 0;
}

// ---------------------------------------------------------
// 初始化
// ---------------------------------------------------------
void initGL()
{
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);

    for (int i = 0; i < 256; i++) keys[i] = false;

    whiteTextureID = CreateWhiteTexture();

    ourShader = new Shader(V_SHADER_SOURCE, F_SHADER_SOURCE, true);
    depthShader = new Shader(DEPTH_VS_SOURCE, DEPTH_FS_SOURCE, true);

    InitSnowSystem();

    glGenFramebuffers(1, &depthMapFBO);
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    std::cout << "Loading terrain..." << std::endl;
    terrainModel = new Model("../Models/mountain.quads.obj");

    std::cout << "Loading Skybox..." << std::endl;
    skyboxModel = new Model("../Models/aurora-sky.gltf");

    std::cout << "Loading animated T-Rex..." << std::endl;
    Assimp::Importer importer;
    std::string dinoPath = "../Models/Tyranno/Tyranno_Walk.glb";
    const aiScene* scene = importer.ReadFile(dinoPath, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

    if (!scene || !scene->mRootNode || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) {
        std::cout << "ERROR::ASSIMP (T-Rex):: " << importer.GetErrorString() << std::endl;
    }
    else {
        animatedModel = new Model();
        std::string dinoDir = dinoPath.substr(0, dinoPath.find_last_of('/'));
        animatedModel->LoadFromScene(scene, dinoDir);
        std::cout << "T-Rex loaded." << std::endl;

        if (scene->HasAnimations()) {
            animation = new Animation(scene, animatedModel);
            animator = new Animator(animation);
        }
    }

    ourShader->use();
    ourShader->setInt("texture_diffuse1", 0);
    ourShader->setInt("texture_specular1", 1);
    ourShader->setInt("texture_normal1", 2);
    ourShader->setInt("shadowMap", 3);
}

void processInput(float dt)
{
    if (keys['w'] || keys['W']) camera.ProcessKeyboard(FORWARD, dt);
    if (keys['s'] || keys['S']) camera.ProcessKeyboard(BACKWARD, dt);
    if (keys['a'] || keys['A']) camera.ProcessKeyboard(LEFT, dt);
    if (keys['d'] || keys['D']) camera.ProcessKeyboard(RIGHT, dt);
    if (keys['x'] || keys['X']) camera.ProcessKeyboard(UP, dt);
    if (keys['z'] || keys['Z']) camera.ProcessKeyboard(DOWN, dt);

    // [修改] K/L 键控制旋转
    float turnSpeed = 20.0f;
    if (keys['k'] || keys['K']) {
        dinoRotationAngle += turnSpeed * dt; // 左转
    }
    if (keys['l'] || keys['L']) {
        dinoRotationAngle -= turnSpeed * dt; // 右转
    }

}

// ---------------------------------------------------------
// 渲染核心
// ---------------------------------------------------------
void renderScene(Shader& shader)
{
    // 地形
    glm::mat4 modelTerrain = glm::mat4(10.0f);
    // [修复] 将地形高度从错误的 4.0f 改回 -20.0f，让恐龙站在地面上
    modelTerrain = glm::translate(modelTerrain, glm::vec3(0.0f, 4.0f, 0.0f));
    modelTerrain = glm::scale(modelTerrain, glm::vec3(modelScale));
    shader.setMat4("model", modelTerrain);

    std::vector<glm::mat4> identityMatrices(100, glm::mat4(1.0f));
    glUniformMatrix4fv(glGetUniformLocation(shader.ID, "finalBonesMatrices"), 100, GL_FALSE, glm::value_ptr(identityMatrices[0]));

    if (terrainModel) terrainModel->Draw(shader);

    // 恐龙
    glm::mat4 modelDino = glm::mat4(1.0f);
    modelDino = glm::translate(modelDino, dinoPos);
    // [修改] 使用动态角度 dinoRotationAngle 替代写死的 -60.0f
    modelDino = glm::rotate(modelDino, glm::radians(dinoRotationAngle), glm::vec3(0.0f, 1.0f, 0.0f));
    modelDino = glm::scale(modelDino, glm::vec3(0.1f));
    shader.setMat4("model", modelDino);

    if (animator) {
        auto transforms = animator->GetFinalBoneMatrices();
        glUniformMatrix4fv(glGetUniformLocation(shader.ID, "finalBonesMatrices"), (GLsizei)transforms.size(), GL_FALSE, glm::value_ptr(transforms[0]));
    }
    else {
        glUniformMatrix4fv(glGetUniformLocation(shader.ID, "finalBonesMatrices"), 100, GL_FALSE, glm::value_ptr(identityMatrices[0]));
    }

    if (animatedModel) animatedModel->Draw(shader);
}

// ---------------------------------------------------------
// 渲染循环
// ---------------------------------------------------------
// ---------------------------------------------------------
// 渲染循环 (修改了移动方向计算逻辑)
// ---------------------------------------------------------
void display()
{
    float currentFrame = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    processInput(deltaTime);

    if (animator) {
        // [修改] 修复行进方向与朝向不一致的问题
        glm::vec3 forward;

        // ！！！如果恐龙横着走或倒着走，修改这个 correction 值！！！
        // 常见值：
        // 0.0f   : 默认
        // 90.0f  : 解决横着走 (向右)
        // -90.0f : 解决横着走 (向左)
        // 180.0f : 解决倒着走 (太空步)
        float correction = 90.0f;

        // 计算带修正的前进方向
        forward.x = sin(glm::radians(dinoRotationAngle + correction));
        forward.y = 0.0f;
        forward.z = cos(glm::radians(dinoRotationAngle + correction));

        dinoForward = glm::normalize(forward);

        // 只有当按下按键时才移动，或者保持一直移动？
        // 你现在的逻辑是一直移动。如果想按键才走，可以在 processInput 里设置一个 flag。
        dinoPos += dinoForward * dinoSpeed * deltaTime;

        animator->UpdateAnimation(deltaTime);
    }

    // =============================================================
    // 1. Shadow Pass: 渲染深度图 (从光源视角)
    // =============================================================

    glm::mat4 lightProjection = glm::ortho(-300.0f, 300.0f, -300.0f, 300.0f, 1.0f, 500.0f);
    glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 lightSpaceMatrix = lightProjection * lightView;

    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    depthShader->use();
    depthShader->setMat4("lightSpaceMatrix", lightSpaceMatrix);
    renderScene(*depthShader);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // =============================================================
    // 2. Lighting Pass: 正常渲染 (从相机视角)
    // =============================================================

    glViewport(0, 0, screenWidth, screenHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ourShader->use();

    glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)screenWidth / (float)screenHeight, 0.1f, 3000.0f);
    glm::mat4 view = camera.GetViewMatrix();

    ourShader->setMat4("projection", projection);
    ourShader->setMat4("view", view);
    ourShader->setVec3("viewPos", camera.Position);
    ourShader->setVec3("lightPos", lightPos);
    ourShader->setMat4("lightSpaceMatrix", lightSpaceMatrix);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, depthMap);

    // 天空盒
    if (skyboxModel)
    {
        glDepthMask(GL_FALSE);
        ourShader->setBool("enableLighting", false);

        glm::mat4 skyModel = glm::mat4(1.0f);
        skyModel = glm::translate(skyModel, camera.Position);
        skyModel = glm::rotate(skyModel, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        skyModel = glm::rotate(skyModel, glm::radians(120.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        skyModel = glm::scale(skyModel, glm::vec3(5.0f));

        ourShader->setMat4("model", skyModel);
        std::vector<glm::mat4> identityMatrices(100, glm::mat4(1.0f));
        glUniformMatrix4fv(glGetUniformLocation(ourShader->ID, "finalBonesMatrices"), 100, GL_FALSE, glm::value_ptr(identityMatrices[0]));

        skyboxModel->Draw(*ourShader);
        glDepthMask(GL_TRUE);
    }

    // 场景
    ourShader->setBool("enableLighting", true);
    ourShader->setVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));
    renderScene(*ourShader);

    // 雪花
    RenderSnow();

    glutSwapBuffers();
}

// ---------------------------------------------------------
// 辅助功能
// ---------------------------------------------------------
void reshape(int w, int h) {
    if (h == 0) h = 1;
    screenWidth = w; screenHeight = h;
}
void keyboard(unsigned char key, int x, int y) {
    if (key >= 'A' && key <= 'Z') key = key + 32;
    keys[key] = true;
    if (key == 27) glutLeaveMainLoop();
}
void keyboardUp(unsigned char key, int x, int y) {
    if (key >= 'A' && key <= 'Z') key = key + 32;
    keys[key] = false;
}
void mouseMove(int x, int y) {
    if (firstMouse) { lastX = x; lastY = y; firstMouse = false; }
    float xoffset = x - lastX; float yoffset = lastY - y;
    lastX = x; lastY = y;
    camera.ProcessMouseMovement(xoffset, yoffset);
    if (x != screenWidth / 2 || y != screenHeight / 2) {
        glutWarpPointer(screenWidth / 2, screenHeight / 2);
        lastX = screenWidth / 2; lastY = screenHeight / 2;
    }
}
void idle() { glutPostRedisplay(); }

unsigned int CreateWhiteTexture() {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    unsigned char whiteData[] = { 255, 255, 255, 255 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whiteData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return textureID;
}

unsigned int CreateSnowTexture() {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    const int size = 64;
    unsigned char data[size * size * 4];
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = (float)x / size - 0.5f;
            float dy = (float)y / size - 0.5f;
            float dist = sqrt(dx * dx + dy * dy);
            unsigned char alpha = 0;
            if (dist < 0.5f) {
                float t = 1.0f - (dist / 0.5f);
                alpha = (unsigned char)(t * 255);
            }
            int index = (y * size + x) * 4;
            data[index] = 255; data[index + 1] = 255; data[index + 2] = 255; data[index + 3] = alpha;
        }
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return textureID;
}

void InitSnowSystem() {
    snowShader = new Shader(SNOW_VS_SOURCE, SNOW_FS_SOURCE, true);
    snowTextureID = CreateSnowTexture();
    snowflakes.resize(SNOW_COUNT);
    for (int i = 0; i < SNOW_COUNT; i++) {
        float x = ((rand() % 1000) / 1000.0f * 2.0f - 1.0f) * SNOW_RANGE;
        float y = ((rand() % 1000) / 1000.0f) * 80.0f;
        float z = ((rand() % 1000) / 1000.0f * 2.0f - 1.0f) * SNOW_RANGE;
        snowflakes[i].position = glm::vec3(x, y, z);
        snowflakes[i].velocity = glm::vec3(0.5f + (rand() % 100) / 100.0f, -2.5f - (rand() % 100) / 50.0f, 0.5f + (rand() % 100) / 100.0f);
        snowflakes[i].size = 2.0f + (rand() % 100) / 20.0f;
    }
    glGenVertexArrays(1, &snowVAO);
    glGenBuffers(1, &snowVBO);
    glBindVertexArray(snowVAO);
    glBindBuffer(GL_ARRAY_BUFFER, snowVBO);
    glBufferData(GL_ARRAY_BUFFER, SNOW_COUNT * sizeof(Snowflake), NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Snowflake), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(Snowflake), (void*)offsetof(Snowflake, size));
    glBindVertexArray(0);
}

void RenderSnow() {
    if (!snowShader) return;
    for (int i = 0; i < SNOW_COUNT; i++) {
        snowflakes[i].position += snowflakes[i].velocity * deltaTime;
        glm::vec3 offset = snowflakes[i].position - camera.Position;
        if (offset.y < -30.0f) snowflakes[i].position.y += 80.0f;
        if (offset.x > SNOW_RANGE) snowflakes[i].position.x -= SNOW_RANGE * 2.0f;
        if (offset.x < -SNOW_RANGE) snowflakes[i].position.x += SNOW_RANGE * 2.0f;
        if (offset.z > SNOW_RANGE) snowflakes[i].position.z -= SNOW_RANGE * 2.0f;
        if (offset.z < -SNOW_RANGE) snowflakes[i].position.z += SNOW_RANGE * 2.0f;
    }
    glBindBuffer(GL_ARRAY_BUFFER, snowVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, SNOW_COUNT * sizeof(Snowflake), &snowflakes[0]);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glEnable(GL_PROGRAM_POINT_SIZE);
    snowShader->use();
    glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)screenWidth / (float)screenHeight, 0.1f, 3000.0f);
    glm::mat4 view = camera.GetViewMatrix();
    snowShader->setMat4("projection", projection);
    snowShader->setMat4("view", view);
    snowShader->setInt("spriteTexture", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, snowTextureID);
    glBindVertexArray(snowVAO);
    glDrawArrays(GL_POINTS, 0, SNOW_COUNT);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

unsigned int TextureFromFile(const char* path, const std::string& directory) {
    std::string filename = std::string(path);
    std::replace(filename.begin(), filename.end(), '\\', '/');
    std::string full_path = directory + '/' + filename;
    unsigned int textureID;
    glGenTextures(1, &textureID);
    int width, height, nrComponents;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* data = stbi_load(full_path.c_str(), &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format;
        if (nrComponents == 1) format = GL_RED;
        else if (nrComponents == 3) format = GL_RGB;
        else if (nrComponents == 4) format = GL_RGBA;
        glBindTexture(GL_TEXTURE_2D, textureID);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        stbi_image_free(data);
    }
    else {
        std::cout << "Texture failed: " << full_path << ". Using fallback." << std::endl;
        stbi_image_free(data);
        glDeleteTextures(1, &textureID);
        return whiteTextureID;
    }
    return textureID;
}

unsigned int TextureFromEmbedded(const aiTexture* tex) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    int width, height, nrComponents;
    unsigned char* data = nullptr;
    if (tex->mHeight == 0) data = stbi_load_from_memory(reinterpret_cast<unsigned char*>(tex->pcData), tex->mWidth, &width, &height, &nrComponents, 0);
    else { data = reinterpret_cast<unsigned char*>(tex->pcData); width = tex->mWidth; height = tex->mHeight; nrComponents = 4; }
    if (data) {
        GLenum format = GL_RGB;
        if (nrComponents == 1) format = GL_RED;
        else if (nrComponents == 4) format = GL_RGBA;
        glBindTexture(GL_TEXTURE_2D, textureID);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        if (tex->mHeight == 0) stbi_image_free(data);
    }
    else {
        std::cout << "Embedded texture failed." << std::endl;
        glDeleteTextures(1, &textureID);
        return whiteTextureID;
    }
    return textureID;
}