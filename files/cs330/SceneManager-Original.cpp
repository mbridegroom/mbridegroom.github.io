///////////////////////////////////////////////////////////////////////////////
// scenemanager.cpp
// ================
// This file contains the implementation of the `SceneManager` class, which is 
// responsible for managing the preparation and rendering of 3D scenes. It 
// handles textures, materials, lighting configurations, and object rendering.
//
// AUTHOR: Matthew Bridegroom
// INSTITUTION: Southern New Hampshire University (SNHU)
// COURSE: CS-330 Computational Graphics and Visualization
//
// INITIAL VERSION: November 1, 2023
// LAST REVISED: June 22nd, 2025
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

// declaration of global variables
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
 *
 *  The constructor for the class
 ***********************************************************/
SceneManager::SceneManager(ShaderManager *pShaderManager)
{
	m_pShaderManager = pShaderManager;
	m_basicMeshes = new ShapeMeshes();
}

/***********************************************************
 *  ~SceneManager()
 *
 *  The destructor for the class
 ***********************************************************/
SceneManager::~SceneManager()
{
	m_pShaderManager = NULL;
	delete m_basicMeshes;
	m_basicMeshes = NULL;
}

/***********************************************************
 *  CreateGLTexture()
 *
 *  This method is used for loading textures from image files,
 *  configuring the texture mapping parameters in OpenGL,
 *  generating the mipmaps, and loading the read texture into
 *  the next available texture slot in memory.
 ***********************************************************/
bool SceneManager::CreateGLTexture(const char* filename, std::string tag)
{
	int width = 0;
	int height = 0;
	int colorChannels = 0;
	GLuint textureID = 0;

	// indicate to always flip images vertically when loaded
	stbi_set_flip_vertically_on_load(true);

	// try to parse the image data from the specified image file
	unsigned char* image = stbi_load(
		filename,
		&width,
		&height,
		&colorChannels,
		0);

	// if the image was successfully read from the image file
	if (image)
	{
		std::cout << "Successfully loaded image:" << filename << ", width:" << width << ", height:" << height << ", channels:" << colorChannels << std::endl;

		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_2D, textureID);

		// set the texture wrapping parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		// set texture filtering parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// if the loaded image is in RGB format
		if (colorChannels == 3)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
		// if the loaded image is in RGBA format - it supports transparency
		else if (colorChannels == 4)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
		else
		{
			std::cout << "Not implemented to handle image with " << colorChannels << " channels" << std::endl;
			return false;
		}

		// generate the texture mipmaps for mapping textures to lower resolutions
		glGenerateMipmap(GL_TEXTURE_2D);

		// free the image data from local memory
		stbi_image_free(image);
		glBindTexture(GL_TEXTURE_2D, 0); // Unbind the texture

		// register the loaded texture and associate it with the special tag string
		m_textureIDs[m_loadedTextures].ID = textureID;
		m_textureIDs[m_loadedTextures].tag = tag;
		m_loadedTextures++;

		return true;
	}

	std::cout << "Could not load image:" << filename << std::endl;

	// Error loading the image
	return false;
}

/***********************************************************
 *  BindGLTextures()
 *
 *  This method is used for binding the loaded textures to
 *  OpenGL texture memory slots.  There are up to 16 slots.
 ***********************************************************/
void SceneManager::BindGLTextures()
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		// bind textures on corresponding texture units
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, m_textureIDs[i].ID);
	}
}

/***********************************************************
 *  DestroyGLTextures()
 *
 *  This method is used for freeing the memory in all the
 *  used texture memory slots.
 ***********************************************************/
void SceneManager::DestroyGLTextures()
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		glGenTextures(1, &m_textureIDs[i].ID);
	}
}

/***********************************************************
 *  FindTextureID()
 *
 *  This method is used for getting an ID for the previously
 *  loaded texture bitmap associated with the passed in tag.
 ***********************************************************/
int SceneManager::FindTextureID(std::string tag)
{
	int textureID = -1;
	int index = 0;
	bool bFound = false;

	while ((index < m_loadedTextures) && (bFound == false))
	{
		if (m_textureIDs[index].tag.compare(tag) == 0)
		{
			textureID = m_textureIDs[index].ID;
			bFound = true;
		}
		else
			index++;
	}

	return(textureID);
}

/***********************************************************
 *  FindTextureSlot()
 *
 *  This method is used for getting a slot index for the previously
 *  loaded texture bitmap associated with the passed in tag.
 ***********************************************************/
int SceneManager::FindTextureSlot(std::string tag)
{
	int textureSlot = -1;
	int index = 0;
	bool bFound = false;

	while ((index < m_loadedTextures) && (bFound == false))
	{
		if (m_textureIDs[index].tag.compare(tag) == 0)
		{
			textureSlot = index;
			bFound = true;
		}
		else
			index++;
	}

	return(textureSlot);
}

/***********************************************************
 *  FindMaterial()
 *
 *  This method is used for getting a material from the previously
 *  defined materials list that is associated with the passed in tag.
 ***********************************************************/
bool SceneManager::FindMaterial(std::string tag, OBJECT_MATERIAL& material)
{
	if (m_objectMaterials.size() == 0)
	{
		return(false);
	}

	int index = 0;
	bool bFound = false;
	while ((index < m_objectMaterials.size()) && (bFound == false))
	{
		if (m_objectMaterials[index].tag.compare(tag) == 0)
		{
			bFound = true;
			material.diffuseColor = m_objectMaterials[index].diffuseColor;
			material.specularColor = m_objectMaterials[index].specularColor;
			material.shininess = m_objectMaterials[index].shininess;
		}
		else
		{
			index++;
		}
	}

	return(true);
}

/***********************************************************
 *  SetTransformations()
 *
 *  This method is used for setting the transform buffer
 *  using the passed in transformation values.
 ***********************************************************/
void SceneManager::SetTransformations(
	glm::vec3 scaleXYZ,
	float XrotationDegrees,
	float YrotationDegrees,
	float ZrotationDegrees,
	glm::vec3 positionXYZ)
{
	// variables for this method
	glm::mat4 modelView;
	glm::mat4 scale;
	glm::mat4 rotationX;
	glm::mat4 rotationY;
	glm::mat4 rotationZ;
	glm::mat4 translation;

	// set the scale value in the transform buffer
	scale = glm::scale(scaleXYZ);
	// set the rotation values in the transform buffer
	rotationX = glm::rotate(glm::radians(XrotationDegrees), glm::vec3(1.0f, 0.0f, 0.0f));
	rotationY = glm::rotate(glm::radians(YrotationDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
	rotationZ = glm::rotate(glm::radians(ZrotationDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
	// set the translation value in the transform buffer
	translation = glm::translate(positionXYZ);

	modelView = translation * rotationZ * rotationY * rotationX * scale;

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setMat4Value(g_ModelName, modelView);
	}
}

/***********************************************************
 *  SetShaderColor()
 *
 *  This method is used for setting the passed in color
 *  into the shader for the next draw command
 ***********************************************************/
void SceneManager::SetShaderColor(
	float redColorValue,
	float greenColorValue,
	float blueColorValue,
	float alphaValue)
{
	// variables for this method
	glm::vec4 currentColor;

	currentColor.r = redColorValue;
	currentColor.g = greenColorValue;
	currentColor.b = blueColorValue;
	currentColor.a = alphaValue;

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_UseTextureName, false);
		m_pShaderManager->setVec4Value(g_ColorValueName, currentColor);
	}
}

/***********************************************************
 *  SetShaderTexture()
 *
 *  This method is used for setting the texture data
 *  associated with the passed in ID into the shader.
 ***********************************************************/
void SceneManager::SetShaderTexture(
	std::string textureTag)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_UseTextureName, true);

		int textureID = -1;
		textureID = FindTextureSlot(textureTag);
		m_pShaderManager->setSampler2DValue(g_TextureValueName, textureID);
	}
}

/***********************************************************
 *  SetTextureUVScale()
 *
 *  This method is used for setting the texture UV scale
 *  values into the shader.
 ***********************************************************/
void SceneManager::SetTextureUVScale(float u, float v)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setVec2Value("UVscale", glm::vec2(u, v));
	}
}

/***********************************************************
 *  SetShaderMaterial()
 *
 *  This method is used for passing the material values
 *  into the shader.
 ***********************************************************/
void SceneManager::SetShaderMaterial(
	std::string materialTag)
{
	if (m_objectMaterials.size() > 0)
	{
		OBJECT_MATERIAL material;
		bool bReturn = false;

		bReturn = FindMaterial(materialTag, material);
		if (bReturn == true)
		{
			m_pShaderManager->setVec3Value("material.diffuseColor", material.diffuseColor);
			m_pShaderManager->setVec3Value("material.specularColor", material.specularColor);
			m_pShaderManager->setFloatValue("material.shininess", material.shininess);
		}
	}
}


/***********************************************************
 *  DefineObjectMaterials()
 *
 *  Configures material settings for objects in the 3D scene.
 ***********************************************************/
void SceneManager::DefineObjectMaterials()
{
	OBJECT_MATERIAL grassMaterial;
	grassMaterial.tag = "grass1";
	grassMaterial.diffuseColor = glm::vec3(0.1f, 0.3f, 0.1f);
	grassMaterial.specularColor = glm::vec3(0.1f, 0.1f, 0.1f);
	grassMaterial.shininess = 0.5f;
	m_objectMaterials.push_back(grassMaterial);

	OBJECT_MATERIAL concreteMaterial;
	concreteMaterial.tag = "concrete1";
	concreteMaterial.diffuseColor = glm::vec3(0.5f, 0.5f, 0.5f);
	concreteMaterial.specularColor = glm::vec3(0.3f, 0.3f, 0.3f);
	concreteMaterial.shininess = 8.5f;
	m_objectMaterials.push_back(concreteMaterial);

	OBJECT_MATERIAL pipeMaterial;
	pipeMaterial.tag = "pipe1";
	pipeMaterial.diffuseColor = glm::vec3(0.4f, 0.4f, 0.5f);
	pipeMaterial.specularColor = glm::vec3(0.8f, 0.8f, 0.8f);
	pipeMaterial.shininess = 256.0f;
	m_objectMaterials.push_back(pipeMaterial);

	OBJECT_MATERIAL steelMaterial;
	steelMaterial.tag = "steel1";
	steelMaterial.diffuseColor = glm::vec3(0.4f, 0.4f, 0.5f);
	steelMaterial.specularColor = glm::vec3(0.8f, 0.8f, 0.8f);
	steelMaterial.shininess = 256.0f;
	m_objectMaterials.push_back(steelMaterial);
}

/***********************************************************
 *  SetupSceneLights()
 *
 *  Adds and configures light sources for the 3D scene.
 ***********************************************************/
void SceneManager::SetupSceneLights()
{
	// Enable custom lighting
	m_pShaderManager->setBoolValue(g_UseLightingName, true);

	

	// Spot Light 1 - Sun
	m_pShaderManager->setVec3Value("directionalLight.direction", -0.707f, -0.707f, 0.0f);
	m_pShaderManager->setVec3Value("directionalLight.ambient", 0.5f, 0.5f, 0.5f); 
	m_pShaderManager->setVec3Value("directionalLight.diffuse", 1.0f, 0.95f, 0.8f); 
	m_pShaderManager->setVec3Value("directionalLight.specular", 1.2f, 1.1f, 0.9f); 
	m_pShaderManager->setBoolValue("directionalLight.bActive", true);
	
	// Point Light 1
	m_pShaderManager->setVec3Value("pointLights[0].position", 2.0f, 4.0f, 2.0f);
	m_pShaderManager->setVec3Value("pointLights[0].ambient", 0.1f, 0.1f, 0.1f);
	m_pShaderManager->setVec3Value("pointLights[0].diffuse", 0.8f, 0.8f, 0.8f);
	m_pShaderManager->setVec3Value("pointLights[0].specular", 1.0f, 1.0f, 1.0f);
	m_pShaderManager->setFloatValue("pointLights[0].constant", 1.0f);
	m_pShaderManager->setFloatValue("pointLights[0].linear", 0.14f);
	m_pShaderManager->setFloatValue("pointLights[0].quadratic", 0.07f);
	m_pShaderManager->setBoolValue("pointLights[0].bActive", true);


	// Point Light 2
	m_pShaderManager->setVec3Value("pointLights[1].position", -2.0f, 4.0f, 2.0f);
	m_pShaderManager->setVec3Value("pointLights[1].ambient", 0.1f, 0.1f, 0.1f);
	m_pShaderManager->setVec3Value("pointLights[1].diffuse", 0.8f, 0.8f, 0.8f);
	m_pShaderManager->setVec3Value("pointLights[1].specular", 1.0f, 1.0f, 1.0f);
	m_pShaderManager->setFloatValue("pointLights[1].constant", 1.0f);
	m_pShaderManager->setFloatValue("pointLights[1].linear", 0.14f);
	m_pShaderManager->setFloatValue("pointLights[1].quadratic", 0.07f);
	m_pShaderManager->setBoolValue("pointLights[1].bActive", true);

}

/**************************************************************/
/*** STUDENTS CAN MODIFY the code in the methods BELOW for  ***/
/*** preparing and rendering their own 3D replicated scenes.***/
/*** Please refer to the code in the OpenGL sample project  ***/
/*** for assistance.                                        ***/
/**************************************************************/
void SceneManager::LoadSceneTextures()
{                              

	// create the textures
	bool bReturn = false;
	bReturn = CreateGLTexture(
		"textures/pipe.jpg",
		"pipe");
	bReturn = CreateGLTexture(
		"textures/grass.jpg",
		"grass");
	bReturn = CreateGLTexture(
		"textures/concrete.jpg",
		"concrete");
	bReturn = CreateGLTexture(
		"textures/steel.jpg",
		"steel");

	BindGLTextures();
}

/***********************************************************
 *  PrepareScene()
 *
 *  This method is used for preparing the 3D scene by loading
 *  the shapes, textures in memory to support the 3D scene 
 *  rendering
 ***********************************************************/
void SceneManager::PrepareScene()
{

	LoadSceneTextures();
	DefineObjectMaterials();
	SetupSceneLights();
	// only one instance of a particular mesh needs to be
	// loaded in memory no matter how many times it is drawn
	// in the rendered 3D scene


	m_basicMeshes->LoadPlaneMesh();
	m_basicMeshes->LoadTorusMesh(); // Load the torus mesh for the ring
	m_basicMeshes->LoadBoxMesh();   // For the legs
	m_basicMeshes->LoadCylinderMesh();
	m_basicMeshes->LoadSphereMesh();
	m_basicMeshes->LoadPyramid3Mesh();
}

/***********************************************************
 *  RenderScene()
 *
 *  This method is used for rendering the 3D scene by 
 *  transforming and drawing the basic 3D shapes
 ***********************************************************/
void SceneManager::RenderScene()
{
	// declare the variables for the transformations
	glm::vec3 scaleXYZ;
	float XrotationDegrees = 0.0f;
	float YrotationDegrees = 0.0f;
	float ZrotationDegrees = 0.0f;
	glm::vec3 positionXYZ;

	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	
	// Set the XYZ scale for the plane to encompass all objects
	scaleXYZ = glm::vec3(100.0f, 1.0f, 100.0f); // Large enough to cover the legs positions
	// Set the XYZ rotation for the plane
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	// Set the XYZ position for the plane to act as the ground
	positionXYZ = glm::vec3(0.0f, 0.0f, 0.0f); // Positioned at y=0 to be below all objects
	// Set the transformations for the plane
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);
	// Set a green color for the plane
	SetShaderTexture("grass");
	SetShaderMaterial("grass1");
	SetTextureUVScale(150.0f, 150.0f);
	m_basicMeshes->DrawPlaneMesh();



	// --- Render the torus (ring) ---
	scaleXYZ = glm::vec3(2.0f, 2.0f, 1.0f); // Position above the floor
	positionXYZ = glm::vec3(0.0f, 3.0f, 0.0f);
	XrotationDegrees = 90.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	SetShaderTexture("concrete");
	SetShaderMaterial("concrete1");
	SetTextureUVScale(20.0f, 10.0f);
	m_basicMeshes->DrawTorusMesh();


	// Leg 1
	{
		scaleXYZ = glm::vec3(0.5f, 3.0f, 0.5f);
		positionXYZ = glm::vec3(2.0f, 1.5f, 0.0f);
		XrotationDegrees = 0.0f;
		YrotationDegrees = 0.0f;// Alignment towards torus
		ZrotationDegrees = 15.0f;
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		SetShaderTexture("concrete");
		SetShaderMaterial("concrete1");

		SetTextureUVScale(2.0f, 8.0f);
		m_basicMeshes->DrawBoxMesh();

		// Cylinder (Pipe)
		scaleXYZ = glm::vec3(0.05f, 2.8f, 0.05f); // Thin cylinder
		positionXYZ = glm::vec3(2.6f, 0.2f, 0.0f); // Offset to right side of pillar
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		SetShaderTexture("pipe");
		SetShaderMaterial("pipe1");
		SetTextureUVScale(0.4f, 0.4f); // Elongate texture vertically
		m_basicMeshes->DrawCylinderMesh();
	}

	// Leg 2
	{
		scaleXYZ = glm::vec3(0.5f, 3.0f, 0.5f);
		positionXYZ = glm::vec3(1.0f, 1.5f, 1.7321f);
		XrotationDegrees = -15.0f;
		YrotationDegrees = 30.0f;// Alignment towards torus
		ZrotationDegrees = 0.0f;
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		SetShaderTexture("concrete");
		SetShaderMaterial("concrete1");
		SetTextureUVScale(2.0f, 8.0f);
		m_basicMeshes->DrawBoxMesh();




	}

	// Leg 3
	{
		scaleXYZ = glm::vec3(0.5f, 3.0f, 0.5f);
		positionXYZ = glm::vec3(-1.0f, 1.5f, 1.7321f);
		XrotationDegrees = 15.0f;
		YrotationDegrees = 150.0f;// Alignment towards torus
		ZrotationDegrees = 0.0f;
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		SetShaderTexture("concrete");
		SetShaderMaterial("concrete1");
		SetTextureUVScale(2.0f, 8.0f);
		m_basicMeshes->DrawBoxMesh();
	}

	// Leg 4
	{
		scaleXYZ = glm::vec3(0.5f, 3.0f, 0.5f);
		positionXYZ = glm::vec3(-2.0f, 1.5f, 0.0f);
		XrotationDegrees = 0.0f;
		YrotationDegrees = 90.0f;// Alignment towards torus
		ZrotationDegrees = -15.0f;
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		SetShaderTexture("concrete");
		SetShaderMaterial("concrete1");
		SetTextureUVScale(2.0f, 8.0f);
		m_basicMeshes->DrawBoxMesh();
	}

	// Leg 5
	{
		scaleXYZ = glm::vec3(0.5f, 3.0f, 0.5f);
		positionXYZ = glm::vec3(-1.0f, 1.5f, -1.7321f);
		XrotationDegrees = 15.0f;
		YrotationDegrees = 30.0f;// Alignment towards torus
		ZrotationDegrees = 0.0f;
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		SetShaderTexture("concrete");
		SetShaderMaterial("concrete1");
		SetTextureUVScale(2.0f, 8.0f);
		m_basicMeshes->DrawBoxMesh();
	}

	// Leg 6
	{
		scaleXYZ = glm::vec3(0.5f, 3.0f, 0.5f);
		positionXYZ = glm::vec3(1.0f, 1.5f, -1.7321f);
		XrotationDegrees = -15.0f;
		YrotationDegrees = 150.0f;// Alignment towards torus
		ZrotationDegrees = 0.0f;
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		SetShaderTexture("concrete");
		SetShaderMaterial("concrete1");
		SetTextureUVScale(2.0f, 8.0f);
		m_basicMeshes->DrawBoxMesh();
	}

	// Lower Rocket Body
	{
		scaleXYZ = glm::vec3(1.5f, 18.0f, 1.5f);
		positionXYZ = glm::vec3(0.0f, 3.0f, 0.0);
		XrotationDegrees = 0.0f;
		YrotationDegrees = 0.0f;
		ZrotationDegrees = 0.0f;
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		SetShaderTexture("steel");
		SetShaderMaterial("steel1");
		SetTextureUVScale(2.0f, 8.0f);
		m_basicMeshes->DrawCylinderMesh();

		//GridFin1
		scaleXYZ = glm::vec3(0.6f, 0.1f, 0.6f);
		positionXYZ = glm::vec3(1.7f, 20.5f, 0.0f);
		XrotationDegrees = 0.0f;
		YrotationDegrees = 0.0f;
		ZrotationDegrees = 0.0f;
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		SetShaderTexture("steel");
		SetShaderMaterial("steel1");
		SetTextureUVScale(10.0f, 10.0f);
		m_basicMeshes->DrawBoxMesh();
		
		//GridFin2
		scaleXYZ = glm::vec3(0.6f, 0.1f, 0.6f);
		positionXYZ = glm::vec3(-1.7f, 20.5f, 0.0f);
		XrotationDegrees = 0.0f;
		YrotationDegrees = 0.0f;
		ZrotationDegrees = 0.0f;
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		SetShaderTexture("steel");
		SetShaderMaterial("steel1");
		SetTextureUVScale(10.0f, 10.0f);
		m_basicMeshes->DrawBoxMesh();
		
		//GridFin3
		scaleXYZ = glm::vec3(0.6f, 0.1f, 0.6f);
		positionXYZ = glm::vec3(0.0f, 20.5f, 1.7f);
		XrotationDegrees = 0.0f;
		YrotationDegrees = 0.0f;
		ZrotationDegrees = 0.0f;
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		SetShaderTexture("steel");
		SetShaderMaterial("steel1");
		SetTextureUVScale(10.0f, 10.0f);
		m_basicMeshes->DrawBoxMesh();
		
		//GridFin4
		scaleXYZ = glm::vec3(0.6f, 0.1f, 0.6f);
		positionXYZ = glm::vec3(-0.0f, 20.5f, -1.7f);
		XrotationDegrees = 0.0f;
		YrotationDegrees = 0.0f;
		ZrotationDegrees = 0.0f;
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		SetShaderTexture("steel");
		SetShaderMaterial("steel1");
		SetTextureUVScale(10.0f, 10.0f);
		m_basicMeshes->DrawBoxMesh();
	}

	// Interstage
	{
		scaleXYZ = glm::vec3(1.5f, 0.5f, 1.5f);
		positionXYZ = glm::vec3(0.0f, 21.0f, 0.0);
		XrotationDegrees = 0.0f;
		YrotationDegrees = 0.0f;
		ZrotationDegrees = 0.0f;
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		SetShaderTexture("steel");
		SetShaderMaterial("steel1");
		SetTextureUVScale(30.0f, 10.0f);
		m_basicMeshes->DrawCylinderMesh();
	}

	// Upper Rocket Body Tube
	{
		scaleXYZ = glm::vec3(1.5f, 12.0f, 1.5f);
		positionXYZ = glm::vec3(0.0f, 21.5f, 0.0);
		XrotationDegrees = 0.0f;
		YrotationDegrees = 0.0f;
		ZrotationDegrees = 0.0f;
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		SetShaderTexture("steel");
		SetShaderMaterial("steel1");
		SetTextureUVScale(2.0f, 8.0f);
		m_basicMeshes->DrawCylinderMesh();
	}

	// Upper Rocket Body Cone
	{
		scaleXYZ = glm::vec3(1.5f, 4.5f, 1.5f);
		positionXYZ = glm::vec3(0.0f, 33.5f, 0.0);
		XrotationDegrees = 0.0f;
		YrotationDegrees = 0.0f;
		ZrotationDegrees = 0.0f;
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		SetShaderTexture("steel");
		SetShaderMaterial("steel1");
		SetTextureUVScale(2.0f, 8.0f);
		m_basicMeshes->DrawSphereMesh();

		//Lower Flap 1
		scaleXYZ = glm::vec3(1.7f, 3.0f, 0.1f);
		positionXYZ = glm::vec3(2.0f, 23.0f, 0.0f);
		XrotationDegrees = 0.0f;
		YrotationDegrees = 0.0f;
		ZrotationDegrees = 0.0f;
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		SetShaderTexture("steel");
		SetShaderMaterial("steel1");
		SetTextureUVScale(0.5f, 0.5f);
		m_basicMeshes->DrawBoxMesh();

		scaleXYZ = glm::vec3(3.0f, 3.0f, 0.1f);
		positionXYZ = glm::vec3(1.35f, 26.0f, 0.0f);
		XrotationDegrees = 0.0f;
		YrotationDegrees = 0.0f;
		ZrotationDegrees = 0.0f;
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		SetShaderTexture("steel");
		SetShaderMaterial("steel1");
		SetTextureUVScale(0.5f, 0.5f);
		m_basicMeshes->DrawPyramid3Mesh();

		//Lower Flap 2
		scaleXYZ = glm::vec3(1.7f, 3.0f, 0.1f);
		positionXYZ = glm::vec3(-2.0f, 23.0f, 0.0f);
		XrotationDegrees = 0.0f;
		YrotationDegrees = 0.0f;
		ZrotationDegrees = 0.0f;
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		SetShaderTexture("steel");
		SetShaderMaterial("steel1");
		SetTextureUVScale(0.5f, 0.5f);
		m_basicMeshes->DrawBoxMesh();

		scaleXYZ = glm::vec3(3.0f, 3.0f, 0.1f);
		positionXYZ = glm::vec3(-1.35f, 26.0f, 0.0f);
		XrotationDegrees = 0.0f;
		YrotationDegrees = 0.0f;
		ZrotationDegrees = 0.0f;
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		SetShaderTexture("steel");
		SetShaderMaterial("steel1");
		SetTextureUVScale(0.5f, 0.5f);
		m_basicMeshes->DrawPyramid3Mesh();


		//Upper Flap 1
		scaleXYZ = glm::vec3(1.7f, 2.0f, 0.1f);
		positionXYZ = glm::vec3(1.48f, 34.0f, 0.0f);
		XrotationDegrees = 0.0f;
		YrotationDegrees = 0.0f;
		ZrotationDegrees = 10.0f;
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		SetShaderTexture("steel");
		SetShaderMaterial("steel1");
		SetTextureUVScale(0.5f, 0.5f);
		m_basicMeshes->DrawBoxMesh();

		scaleXYZ = glm::vec3(2.0f, 2.0f, 0.1f);
		positionXYZ = glm::vec3(1.0f, 35.9f, 0.0f);
		XrotationDegrees = 0.0f;
		YrotationDegrees = 0.0f;
		ZrotationDegrees = 10.0f;
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		SetShaderTexture("steel");
		SetShaderMaterial("steel1");
		SetTextureUVScale(0.5f, 0.5f);
		m_basicMeshes->DrawPyramid3Mesh();

		//Upper Flap 2
		scaleXYZ = glm::vec3(1.7f, 2.0f, 0.1f);
		positionXYZ = glm::vec3(-1.48f, 34.0f, 0.0f);
		XrotationDegrees = 0.0f;
		YrotationDegrees = 0.0f;
		ZrotationDegrees = -10.0f;
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		SetShaderTexture("steel");
		SetShaderMaterial("steel1");
		SetTextureUVScale(0.5f, 0.5f);
		m_basicMeshes->DrawBoxMesh();

		scaleXYZ = glm::vec3(2.0f, 2.0f, 0.1f);
		positionXYZ = glm::vec3(-1.0f, 35.9f, 0.0f);
		XrotationDegrees = 0.0f;
		YrotationDegrees = 0.0f;
		ZrotationDegrees = -10.0f;
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		SetShaderTexture("steel");
		SetShaderMaterial("steel1");
		SetTextureUVScale(0.5f, 0.5f);
		m_basicMeshes->DrawPyramid3Mesh();
	}
	// Tank Farm
	{
		//Tank1
		scaleXYZ = glm::vec3(1.5f, 15.0f, 1.5f);
		positionXYZ = glm::vec3(-7.5f, 1.5f, -35.5f);
		XrotationDegrees = 90.0f;
		YrotationDegrees = 90.0f;
		ZrotationDegrees = 0.0f;
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		SetShaderTexture("concrete");
		SetShaderMaterial("pipe1");
		SetTextureUVScale(2.0f, 8.0f);
		m_basicMeshes->DrawCylinderMesh();

		//Tank2
		scaleXYZ = glm::vec3(1.5f, 15.0f, 1.5f);
		positionXYZ = glm::vec3(-7.5f, 1.5f, -38.5f);
		XrotationDegrees = 90.0f;
		YrotationDegrees = 90.0f;
		ZrotationDegrees = 0.0f;
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		SetShaderTexture("concrete");
		SetShaderMaterial("pipe1");
		SetTextureUVScale(2.0f, 8.0f);
		m_basicMeshes->DrawCylinderMesh();

		//Tank3
		scaleXYZ = glm::vec3(1.5f, 15.0f, 1.5f);
		positionXYZ = glm::vec3(-7.5f, 1.5f, -41.5f);
		XrotationDegrees = 90.0f;
		YrotationDegrees = 90.0f;
		ZrotationDegrees = 0.0f;
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		SetShaderTexture("concrete");
		SetShaderMaterial("pipe1");
		SetTextureUVScale(2.0f, 8.0f);
		m_basicMeshes->DrawCylinderMesh();

		//Tank4
		scaleXYZ = glm::vec3(1.5f, 15.0f, 1.5f);
		positionXYZ = glm::vec3(-7.5f, 1.5f, -44.5f);
		XrotationDegrees = 90.0f;
		YrotationDegrees = 90.0f;
		ZrotationDegrees = 0.0f;
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		SetShaderTexture("concrete");
		SetShaderMaterial("pipe1");
		SetTextureUVScale(2.0f, 8.0f);
		m_basicMeshes->DrawCylinderMesh();
	}
	// Catch and Launch Tower
	// Tower Base
	{
		glm::vec3 towerBasePos = glm::vec3(0.0f, 1.0f, -6.0f);
		glm::vec3 scaleBase = glm::vec3(4.0f, 2.0f, 4.0f);
		SetTransformations(scaleBase, 0.0f, 0.0f, 0.0f, towerBasePos);
		SetShaderTexture("concrete");
		SetShaderMaterial("concrete1");
		SetTextureUVScale(2.0f, 2.0f);
		m_basicMeshes->DrawBoxMesh();
	}

	// Tower Segments
	int numSegments = 17;
	float segmentHeight = 2.0f;
	float baseY = 2.0f;
	float centerX = 0.0f;
	float centerZ = -6.0f;
	float halfSize = 1.8f;

	for (int i = 0; i < numSegments; ++i) {
		float yOffset = baseY + i * segmentHeight;

		// Corner Posts 
		glm::vec3 scalePost = glm::vec3(0.2f, segmentHeight, 0.2f);
		glm::vec3 corners[4] = {
			glm::vec3(centerX - halfSize, yOffset + segmentHeight / 2, centerZ - halfSize),
			glm::vec3(centerX + halfSize, yOffset + segmentHeight / 2, centerZ - halfSize),
			glm::vec3(centerX + halfSize, yOffset + segmentHeight / 2, centerZ + halfSize),
			glm::vec3(centerX - halfSize, yOffset + segmentHeight / 2, centerZ + halfSize),
		};

		for (int j = 0; j < 4; ++j) {
			SetTransformations(scalePost, 0.0f, 0.0f, 0.0f, corners[j]);
			SetShaderTexture("concrete");
			SetShaderMaterial("concrete1");
			SetTextureUVScale(0.4f, 4.0f);
			m_basicMeshes->DrawBoxMesh();
		}

		{
			glm::vec3 diagScale = glm::vec3(0.15f, segmentHeight * 2.35f, 0.15f);

			struct Brace {
				glm::vec3 position;
				glm::vec3 rotation; 
			};

			std::vector<Brace> braces = {
				// Front face -Z
				{{centerX, yOffset + segmentHeight / 2, centerZ - halfSize}, glm::vec3(0.0f, 0.0f,  45.0f)},
				{{centerX, yOffset + segmentHeight / 2, centerZ - halfSize}, glm::vec3(0.0f, 0.0f, -45.0f)},  // Opposite diagonal

				// Right face +X
				{{centerX + halfSize, yOffset + segmentHeight / 2, centerZ}, glm::vec3(-45.0f, 0.0f, 0.0f)},
				{{centerX + halfSize, yOffset + segmentHeight / 2, centerZ}, glm::vec3(45.0f, 0.0f, 0.0f)},   

				// Back face +Z
				{{centerX, yOffset + segmentHeight / 2, centerZ + halfSize}, glm::vec3(0.0f, 0.0f, -45.0f)},
				{{centerX, yOffset + segmentHeight / 2, centerZ + halfSize}, glm::vec3(0.0f, 0.0f,  45.0f)},  

				// Left face -X
				{{centerX - halfSize, yOffset + segmentHeight / 2, centerZ}, glm::vec3(45.0f, 0.0f, 0.0f)},
				{{centerX - halfSize, yOffset + segmentHeight / 2, centerZ}, glm::vec3(-45.0f, 0.0f, 0.0f)},  
			};

			for (const auto& brace : braces) {
				glm::vec3 scale = diagScale;

				SetTransformations(scale,
					brace.rotation.x,
					brace.rotation.y,
					brace.rotation.z,
					brace.position);
				SetShaderTexture("concrete");
				SetShaderMaterial("concrete1");
				SetTextureUVScale(0.5f, 1.8f);
				m_basicMeshes->DrawBoxMesh();
			}
		}
	}
	//Tower Top
	{
		glm::vec3 towerTopPos = glm::vec3(0.0f, 36.0f, -6.0f);
		glm::vec3 scaleBase = glm::vec3(4.0f, 2.0f, 4.0f);
		SetTransformations(scaleBase, 0.0f, 0.0f, 0.0f, towerTopPos);
		SetShaderTexture("concrete");
		SetShaderMaterial("concrete1");
		SetTextureUVScale(2.0f, 2.0f);
		m_basicMeshes->DrawBoxMesh();
	}
	{
		glm::vec3 towerTopPos = glm::vec3(0.0f, 36.0f, -6.0f);
		glm::vec3 scaleBase = glm::vec3(2.0f, 2.0f, 6.0f);
		SetTransformations(scaleBase, 0.0f, 0.0f, 0.0f, towerTopPos);
		SetShaderTexture("concrete");
		SetShaderMaterial("concrete1");
		SetTextureUVScale(2.0f, 2.0f);
		m_basicMeshes->DrawBoxMesh();
	}

	//TowerArms
	{
		//Right Arm
		scaleXYZ = glm::vec3(0.2f, 2.0f, 10.0f);
		positionXYZ = glm::vec3(3.0f, 32.0f, -3.0f);
		XrotationDegrees = 0.0f;
		YrotationDegrees = 25.0f;
		ZrotationDegrees = 0.0f;
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		SetShaderTexture("concrete");
		SetShaderMaterial("concrete1");
		SetTextureUVScale(2.0f, 2.0f);
		m_basicMeshes->DrawBoxMesh();

		

		//Left
		scaleXYZ = glm::vec3(0.2f, 2.0f, 10.0f);
		positionXYZ = glm::vec3(-3.0f, 32.0f, -3.0f);
		XrotationDegrees = 0.0f;
		YrotationDegrees = -25.0f;
		ZrotationDegrees = 0.0f;
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		SetShaderTexture("concrete");
		SetShaderMaterial("concrete1");
		SetTextureUVScale(2.0f, 2.0f);
		m_basicMeshes->DrawBoxMesh();


	}

	/****************************************************************/
}
