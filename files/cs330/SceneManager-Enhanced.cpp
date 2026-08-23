///////////////////////////////////////////////////////////////////////////////
// scenemanager.cpp
// ================
// This file contains the implementation of the `SceneManager` class, which is 
// responsible for managing the preparation and rendering of 3D scenes. It 
// handles textures, materials, lighting configurations, and object rendering.
//
// AUTHOR: Matthew Bridegroom
// INSTITUTION: Southern New Hampshire University (SNHU)
// COURSE: CS-330 Computational Graphics and Visualization → CS-499 
//
// INITIAL VERSION: November 1, 2023
// FIRST REVISED: June 22nd, 2025
// LAST REVISED: August 2nd, 2026
//
// RESPONSIBILITIES:
// - Load, bind, and manage textures in OpenGL.
// - Define materials and lighting properties for 3D objects.
// - Manage transformations and shader configurations.
// - Render complex 3D scenes using basic meshes.
//
// NOTE: This implementation leverages external libraries like `stb_image` for 
// texture loading and GLM for matrix and vector operations.
///////////////////////////////////////////////////////////////////////////////

#include "SceneManager.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

#include "SceneManager.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

#include <vector>
#include <unordered_map>
#include <string>
#include <iostream>

// Mesh type for data-driven rendering
enum class MeshType
{
    PLANE,
    BOX,
    CYLINDER,
    SPHERE,
    TORUS,
    PYRAMID3
};

// Core Scene Objec
struct SceneObject
{
    MeshType meshType;

    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);   // X, Y, Z in degrees
    glm::vec3 scale = glm::vec3(1.0f);

    std::string textureTag;
    std::string materialTag;

    glm::vec2 uvScale = glm::vec2(1.0f, 1.0f);
};

// More uniform names
namespace
{
    const char* g_ModelName = "model";
    const char* g_ColorValueName = "objectColor";
    const char* g_TextureValueName = "objectTexture";
    const char* g_UseTextureName = "bUseTexture";
    const char* g_UseLightingName = "bUseLighting";
}

/***********************************************************
 *  SceneManager()
 ***********************************************************/
SceneManager::SceneManager(ShaderManager* pShaderManager)
{
    m_pShaderManager = pShaderManager;
    m_basicMeshes = new ShapeMeshes();
}

/***********************************************************
 *  ~SceneManager()
 ***********************************************************/
SceneManager::~SceneManager()
{
    m_pShaderManager = nullptr;
    delete m_basicMeshes;
    m_basicMeshes = nullptr;

    for (auto& pair : m_textureMap)
        glDeleteTextures(1, &pair.second);
    m_textureMap.clear();
}

/***********************************************************
 *  CreateGLTexture() 
 ***********************************************************/
bool SceneManager::CreateGLTexture(const char* filename, std::string tag)
{
    int width = 0, height = 0, colorChannels = 0;
    GLuint textureID = 0;

    stbi_set_flip_vertically_on_load(true);
    unsigned char* image = stbi_load(filename, &width, &height, &colorChannels, 0);

    if (image)
    {
        std::cout << "Loaded: " << filename << " (" << width << "x" << height << ")" << std::endl;

        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        if (colorChannels == 3)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
        else if (colorChannels == 4)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);

        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(image);
        glBindTexture(GL_TEXTURE_2D, 0);

        m_textureMap[tag] = textureID;
        return true;
    }

    std::cout << "Failed to load: " << filename << std::endl;
    return false;
}

/***********************************************************
 *  BindGLTextures()
 ***********************************************************/
void SceneManager::BindGLTextures()
{
    int unit = 0;
    for (const auto& pair : m_textureMap)
    {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, pair.second);
        unit++;
        if (unit >= 16) break;
    }
}

/***********************************************************
 *  FindTextureSlot() & FindTextureID() - O(1)
 ***********************************************************/
int SceneManager::FindTextureSlot(std::string tag)
{
    static std::unordered_map<std::string, int> unitCache;
    if (unitCache.empty())
    {
        int unit = 0;
        for (const auto& p : m_textureMap)
            unitCache[p.first] = unit++;
    }
    auto it = unitCache.find(tag);
    return (it != unitCache.end()) ? it->second : -1;
}

GLuint SceneManager::FindTextureID(std::string tag)
{
    auto it = m_textureMap.find(tag);
    return (it != m_textureMap.end()) ? it->second : 0;
}

/***********************************************************
 *  Material, Transform, Shader helpers 
 ***********************************************************/
bool SceneManager::FindMaterial(std::string tag, OBJECT_MATERIAL& material)
{
    for (const auto& m : m_objectMaterials)
    {
        if (m.tag == tag)
        {
            material = m;
            return true;
        }
    }
    return false;
}

void SceneManager::SetTransformations(glm::vec3 scaleXYZ, float Xrot, float Yrot, float Zrot, glm::vec3 pos)
{
    glm::mat4 model = glm::translate(pos) *
        glm::rotate(glm::radians(Zrot), glm::vec3(0, 0, 1)) *
        glm::rotate(glm::radians(Yrot), glm::vec3(0, 1, 0)) *
        glm::rotate(glm::radians(Xrot), glm::vec3(1, 0, 0)) *
        glm::scale(scaleXYZ);

    if (m_pShaderManager)
        m_pShaderManager->setMat4Value(g_ModelName, model);
}

void SceneManager::SetShaderTexture(std::string textureTag)
{
    if (m_pShaderManager)
    {
        m_pShaderManager->setIntValue(g_UseTextureName, true);
        m_pShaderManager->setSampler2DValue(g_TextureValueName, FindTextureSlot(textureTag));
    }
}

void SceneManager::SetTextureUVScale(float u, float v)
{
    if (m_pShaderManager)
        m_pShaderManager->setVec2Value("UVscale", glm::vec2(u, v));
}

void SceneManager::SetShaderMaterial(std::string materialTag)
{
    OBJECT_MATERIAL mat;
    if (FindMaterial(materialTag, mat))
    {
        m_pShaderManager->setVec3Value("material.diffuseColor", mat.diffuseColor);
        m_pShaderManager->setVec3Value("material.specularColor", mat.specularColor);
        m_pShaderManager->setFloatValue("material.shininess", mat.shininess);
    }
}

void SceneManager::SetShaderColor(float r, float g, float b, float a)
{
    if (m_pShaderManager)
    {
        m_pShaderManager->setIntValue(g_UseTextureName, false);
        m_pShaderManager->setVec4Value(g_ColorValueName, glm::vec4(r, g, b, a));
    }
}

/***********************************************************
 *  DefineObjectMaterials()
 ***********************************************************/
void SceneManager::DefineObjectMaterials()
{
    // Grass
    OBJECT_MATERIAL grass; grass.tag = "grass1"; grass.diffuseColor = glm::vec3(0.1f, 0.3f, 0.1f);
    grass.specularColor = glm::vec3(0.1f); grass.shininess = 0.5f; m_objectMaterials.push_back(grass);

    // Concrete
    OBJECT_MATERIAL conc; conc.tag = "concrete1"; conc.diffuseColor = glm::vec3(0.5f);
    conc.specularColor = glm::vec3(0.3f); conc.shininess = 8.5f; m_objectMaterials.push_back(conc);

    // Pipe
    OBJECT_MATERIAL pipe; pipe.tag = "pipe1"; pipe.diffuseColor = glm::vec3(0.4f, 0.4f, 0.5f);
    pipe.specularColor = glm::vec3(0.8f); pipe.shininess = 256.0f; m_objectMaterials.push_back(pipe);

    // Steel
    OBJECT_MATERIAL steel; steel.tag = "steel1"; steel.diffuseColor = glm::vec3(0.4f, 0.4f, 0.5f);
    steel.specularColor = glm::vec3(0.8f); steel.shininess = 256.0f; m_objectMaterials.push_back(steel);
}

/***********************************************************
 *  SetupSceneLights()
 ***********************************************************/
void SceneManager::SetupSceneLights()
{
    m_pShaderManager->setBoolValue(g_UseLightingName, true);

    // Sun
    m_pShaderManager->setVec3Value("directionalLight.direction", -0.707f, -0.707f, 0.0f);
    m_pShaderManager->setVec3Value("directionalLight.ambient", 0.5f, 0.5f, 0.5f);
    m_pShaderManager->setVec3Value("directionalLight.diffuse", 1.0f, 0.95f, 0.8f);
    m_pShaderManager->setVec3Value("directionalLight.specular", 1.2f, 1.1f, 0.9f);
    m_pShaderManager->setBoolValue("directionalLight.bActive", true);

    // Point lights
    for (int i = 0; i < 2; ++i)
    {
        std::string base = "pointLights[" + std::to_string(i) + "].";
        glm::vec3 pos = (i == 0) ? glm::vec3(2, 4, 2) : glm::vec3(-2, 4, 2);
        m_pShaderManager->setVec3Value(base + "position", pos);
        m_pShaderManager->setVec3Value(base + "ambient", 0.1f);
        m_pShaderManager->setVec3Value(base + "diffuse", 0.8f);
        m_pShaderManager->setVec3Value(base + "specular", 1.0f);
        m_pShaderManager->setFloatValue(base + "constant", 1.0f);
        m_pShaderManager->setFloatValue(base + "linear", 0.14f);
        m_pShaderManager->setFloatValue(base + "quadratic", 0.07f);
        m_pShaderManager->setBoolValue(base + "bActive", true);
    }
}

/***********************************************************
 *  RenderSingleObject() - Core rendering helper
 ***********************************************************/
void SceneManager::RenderSingleObject(const SceneObject& obj)
{
    SetTransformations(obj.scale, obj.rotation.x, obj.rotation.y, obj.rotation.z, obj.position);
    SetShaderTexture(obj.textureTag);
    SetShaderMaterial(obj.materialTag);
    SetTextureUVScale(obj.uvScale.x, obj.uvScale.y);

    switch (obj.meshType)
    {
    case MeshType::PLANE:    m_basicMeshes->DrawPlaneMesh(); break;
    case MeshType::BOX:      m_basicMeshes->DrawBoxMesh(); break;
    case MeshType::CYLINDER: m_basicMeshes->DrawCylinderMesh(); break;
    case MeshType::SPHERE:   m_basicMeshes->DrawSphereMesh(); break;
    case MeshType::TORUS:    m_basicMeshes->DrawTorusMesh(); break;
    case MeshType::PYRAMID3: m_basicMeshes->DrawPyramid3Mesh(); break;
    }
}

/***********************************************************
 *  LoadSceneTextures()
 ***********************************************************/
void SceneManager::LoadSceneTextures()
{
    CreateGLTexture("textures/pipe.jpg", "pipe");
    CreateGLTexture("textures/grass.jpg", "grass");
    CreateGLTexture("textures/concrete.jpg", "concrete");
    CreateGLTexture("textures/steel.jpg", "steel");

    BindGLTextures();
}

/***********************************************************
 *  PrepareScene() - Data-driven object creation
 ***********************************************************/
void SceneManager::PrepareScene()
{
    LoadSceneTextures();
    DefineObjectMaterials();
    SetupSceneLights();

    m_basicMeshes->LoadPlaneMesh();
    m_basicMeshes->LoadTorusMesh();
    m_basicMeshes->LoadBoxMesh();
    m_basicMeshes->LoadCylinderMesh();
    m_basicMeshes->LoadSphereMesh();
    m_basicMeshes->LoadPyramid3Mesh();

    // ====================== SCENE OBJECTS ======================

    // Ground
    m_sceneObjects.push_back({ MeshType::PLANE, glm::vec3(0), glm::vec3(0), glm::vec3(100,1,100),
                             "grass", "grass1", glm::vec2(150,150) });

    // Ring
    m_sceneObjects.push_back({ MeshType::TORUS, glm::vec3(0,3,0), glm::vec3(90,0,0), glm::vec3(2,2,1),
                             "concrete", "concrete1", glm::vec2(20,10) });

    // Rocket Lower Body
    m_sceneObjects.push_back({ MeshType::CYLINDER, glm::vec3(0,3,0), glm::vec3(0), glm::vec3(1.5f,18,1.5f),
                             "steel", "steel1", glm::vec2(2,8) });

    // Interstage
    m_sceneObjects.push_back({ MeshType::CYLINDER, glm::vec3(0,21,0), glm::vec3(0), glm::vec3(1.5f,0.5f,1.5f),
                             "steel", "steel1", glm::vec2(30,10) });

    // Upper Body
    m_sceneObjects.push_back({ MeshType::CYLINDER, glm::vec3(0,21.5f,0), glm::vec3(0), glm::vec3(1.5f,12,1.5f),
                             "steel", "steel1", glm::vec2(2,8) });

    // Nose Cone
    m_sceneObjects.push_back({ MeshType::SPHERE, glm::vec3(0,33.5f,0), glm::vec3(0), glm::vec3(1.5f,4.5f,1.5f),
                             "steel", "steel1", glm::vec2(2,8) });

    // Grid Fins
    float finY = 20.5f;
    m_sceneObjects.push_back({ MeshType::BOX, glm::vec3(1.7f, finY, 0), glm::vec3(0), glm::vec3(0.6f,0.1f,0.6f), "steel", "steel1", glm::vec2(10,10) });
    m_sceneObjects.push_back({ MeshType::BOX, glm::vec3(-1.7f, finY, 0), glm::vec3(0), glm::vec3(0.6f,0.1f,0.6f), "steel", "steel1", glm::vec2(10,10) });
    m_sceneObjects.push_back({ MeshType::BOX, glm::vec3(0, finY, 1.7f), glm::vec3(0), glm::vec3(0.6f,0.1f,0.6f), "steel", "steel1", glm::vec2(10,10) });
    m_sceneObjects.push_back({ MeshType::BOX, glm::vec3(0, finY, -1.7f), glm::vec3(0), glm::vec3(0.6f,0.1f,0.6f), "steel", "steel1", glm::vec2(10,10) });

    // Legs (6 legs)
    std::vector<glm::vec3> legPositions = {
        {2.0f, 1.5f, 0.0f}, {1.0f, 1.5f, 1.7321f}, {-1.0f, 1.5f, 1.7321f},
        {-2.0f, 1.5f, 0.0f}, {-1.0f, 1.5f, -1.7321f}, {1.0f, 1.5f, -1.7321f}
    };
    std::vector<glm::vec3> legRotations = {
        {0,0,15}, {-15,30,0}, {15,150,0},
        {0,90,-15}, {15,30,0}, {-15,150,0}
    };

    for (size_t i = 0; i < legPositions.size(); ++i)
    {
        SceneObject leg;
        leg.meshType = MeshType::BOX;
        leg.scale = glm::vec3(0.5f, 3.0f, 0.5f);
        leg.position = legPositions[i];
        leg.rotation = legRotations[i];
        leg.textureTag = "concrete";
        leg.materialTag = "concrete1";
        leg.uvScale = glm::vec2(2, 8);
        m_sceneObjects.push_back(leg);
    }

    // Tank Farm
    for (int i = 0; i < 4; ++i)
    {
        SceneObject tank;
        tank.meshType = MeshType::CYLINDER;
        tank.scale = glm::vec3(1.5f, 15.0f, 1.5f);
        tank.position = glm::vec3(-7.5f, 1.5f, -35.5f - i * 3.0f);
        tank.rotation = glm::vec3(90, 90, 0);
        tank.textureTag = "concrete";
        tank.materialTag = "pipe1";
        tank.uvScale = glm::vec2(2, 8);
        m_sceneObjects.push_back(tank);
    }

    // Catch & Launch Tower 
    // Base
    m_sceneObjects.push_back({ MeshType::BOX, glm::vec3(0,1,-6), glm::vec3(0), glm::vec3(4,2,4), "concrete", "concrete1", glm::vec2(2,2) });

    // Tower segments (posts)
    for (int i = 0; i < 17; ++i)
    {
        float y = 2.0f + i * 2.0f;
        float hSize = 1.8f;
        glm::vec3 corners[4] = {
            {-hSize, y, -hSize}, {hSize, y, -hSize},
            {hSize, y, hSize}, {-hSize, y, hSize}
        };
        for (auto& c : corners)
        {
            m_sceneObjects.push_back({ MeshType::BOX, c + glm::vec3(0,1,-6), glm::vec3(0), glm::vec3(0.2f,2.0f,0.2f), "concrete", "concrete1", glm::vec2(0.4f,4.0f) });
        }
    }

    // Tower Top
    m_sceneObjects.push_back({ MeshType::BOX, glm::vec3(0,36,-6), glm::vec3(0), glm::vec3(4,2,4), "concrete", "concrete1", glm::vec2(2,2) });

    std::cout << "Scene prepared with " << m_sceneObjects.size() << " objects.\n";
}

/***********************************************************
 *  RenderScene() - data driven loop
 ***********************************************************/
void SceneManager::RenderScene()
{
    for (const auto& obj : m_sceneObjects)
    {
        RenderSingleObject(obj);
    }
}