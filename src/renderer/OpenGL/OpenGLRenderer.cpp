﻿#include "OpenGLRenderer.h"

#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../Model.h"
#include "../../scene/IScene.h"
#include "../../entities/Entity.h"

#include "../../math/Vector.h"
#include "../../math/Angle.h"

#include "OpenGLTexture.h"
#include "OpenGLVAO.h"

#include "../../scene/LightSource.h"

OpenGLRenderer::OpenGLRenderer(IGame & game)
{
	m_Game = &game;
	
	InitializeShaders();
	InitializeSampler();
	InitializeBaseTexture();

}

OpenGLRenderer::~OpenGLRenderer()
{
	if (m_LinearSampler != 0)
		glDeleteSamplers(1, &m_LinearSampler);

	if (m_BaseTexture != 0)
		glDeleteTextures(1, &m_BaseTexture);
}

void OpenGLRenderer::RenderScene(const IScene & scene, const Vector & cameraPosition, const Angle & cameraRotation) const
{
	int width, height;

	glfwGetFramebufferSize(m_Game->GetWindow(), &width, &height);
	float aspect = (float)width / (float)height;

	glViewport(0, 0, width, height);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glDepthFunc(GL_LESS);

	Vector glPos = -ConverToOpenGL(cameraPosition);
	Angle glRot = -ConverToOpenGL(cameraRotation);

	glm::mat4 projection = glm::perspective(90.0f, aspect, 0.25f, 1000.0f);
	glm::mat4 view = glm::mat4(1.0f);
	view = glm::rotate(view, glm::radians(glRot.x), glm::vec3(1.0f, 0.0f, 0.0f));
	view = glm::rotate(view, glm::radians(glRot.y), glm::vec3(0.0f, 1.0f, 0.0f));
	view = glm::rotate(view, glm::radians(glRot.z), glm::vec3(0.0f, 0.0f, 1.0f));
	view = glm::translate(view, glm::vec3(glPos.x, glPos.y, glPos.z));


	RenderObjects(view, projection, scene);

	glfwSwapBuffers(m_Game->GetWindow());
}

void OpenGLRenderer::RenderObjects(const glm::mat4 & view, const glm::mat4 & projection, const IScene & scene) const
{
	glUseProgram(m_Program.GetProgramID());
	auto entities = scene.GetEntitySystem().GetEntities();

	glDisable(GL_TEXTURE_2D);
	for (auto pair : entities)
	{
		glPushMatrix();
		Entity * ent = pair.second;
		_ASSERT(ent != nullptr);

		const Model * pModel = pair.second->GetModel();

		_ASSERT(pModel != nullptr);

		BindMatrices(view, projection, ent);

		for (const Model::Mesh * mesh : pModel->Meshes)
		{
			_ASSERT(mesh != nullptr && mesh->VAOs.size() > 0);

			BindLightSources(scene);
			DrawMesh(*mesh);
		}

	}
}

void OpenGLRenderer::DrawMesh(const Model::Mesh & mesh) const
{
	
	for (size_t i = 0; i < mesh.VAOs.size(); i++)
	{
		OpenGLVAO * vao = static_cast<OpenGLVAO*>(mesh.VAOs[i]);
		_ASSERT(vao != nullptr && vao->ID != 0);

		auto pair = mesh.Materials[i];

		BindTextures(pair.second);

		glBindVertexArray(vao->ID);
		glDrawElements(GL_TRIANGLES, vao->Size, GL_UNSIGNED_INT, (void*)0);
		glBindVertexArray(0);
	}
}

void OpenGLRenderer::BindMatrices(const glm::mat4 & view, const glm::mat4 & projection, const Entity * ent) const
{
	Vector pos = ConverToOpenGL(ent->GetPosition());
	Angle ang = ConverToOpenGL(ent->GetRotation());
	glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(pos.x, pos.y, pos.z));
	model = glm::rotate(model, glm::radians(ang.x), glm::vec3(1.0f, 0.0f, 0.0f));
	model = glm::rotate(model, glm::radians(ang.y), glm::vec3(0.0f, 1.0f, 0.0f));
	model = glm::rotate(model, glm::radians(ang.z), glm::vec3(0.0f, 0.0f, 1.0f));

	glm::mat4 MV = view * model;
	glm::mat4 MVP = projection * MV;
	GLuint MVPLocation = glGetUniformLocation(m_Program.GetProgramID(), "MVP");
	_ASSERT(MVPLocation != -1);

	glUniformMatrix4fv(MVPLocation, 1, GL_FALSE, glm::value_ptr(MVP));
	glUniformMatrix4fv(glGetUniformLocation(m_Program.GetProgramID(), "M"), 1, GL_FALSE, glm::value_ptr(model));
}

void OpenGLRenderer::BindLightSources(const IScene & scene) const
{
	auto sources = scene.GetLightSources(LightSource::SPOT);

	size_t lightCount = (int)fmin(8, sources.size());

	glUniform1i(glGetUniformLocation(m_Program.GetProgramID(), "SpotlightCount"), lightCount);

	for (size_t i = 0; i < lightCount; i++)
	{
		const SpotLightSource * light = static_cast<const SpotLightSource*>(sources[i]);

		_ASSERT(light != nullptr);

		std::string lightName = "Spotlights[" + std::to_string(i) + "]";
		Vector dir = ConverToOpenGL(light->Rotation.ToDirection());
		glUniform3f(glGetUniformLocation(m_Program.GetProgramID(), (lightName + ".Direction").c_str()), dir.x, dir.y, dir.z);
		Vector lightPosition = ConverToOpenGL(light->Position);
		glUniform3f(glGetUniformLocation(m_Program.GetProgramID(), (lightName + ".Position").c_str()), lightPosition.x, lightPosition.y, lightPosition.z);
		glUniform3f(glGetUniformLocation(m_Program.GetProgramID(), (lightName + ".Color").c_str()), light->Color[0], light->Color[1], light->Color[2]);
		glUniform1f(glGetUniformLocation(m_Program.GetProgramID(), (lightName + ".Exponent").c_str()), light->Exponent);
		glUniform1f(glGetUniformLocation(m_Program.GetProgramID(), (lightName + ".Linear").c_str()), light->Attenuation.Linear);
		glUniform1f(glGetUniformLocation(m_Program.GetProgramID(), (lightName + ".Constant").c_str()), light->Attenuation.Constant);
		glUniform1f(glGetUniformLocation(m_Program.GetProgramID(), (lightName + ".Quadratic").c_str()), light->Attenuation.Quadratic);
		glUniform1f(glGetUniformLocation(m_Program.GetProgramID(), (lightName + ".Cone").c_str()), light->Cone);
		glUniform1f(glGetUniformLocation(m_Program.GetProgramID(), (lightName + ".MaxDistance").c_str()), light->MaxDistance);
	}
}

void OpenGLRenderer::BindTextures(const Material * mat) const
{
	glUniform1i(glGetUniformLocation(m_Program.GetProgramID(), "diffuseTexture"), 0);

	glActiveTexture(GL_TEXTURE0);
	_ASSERT(mat != nullptr);
	if (mat->DiffuseTex.get() == nullptr)
	{
		glBindTexture(GL_TEXTURE_2D, m_BaseTexture);
	}
	else
	{
		const OpenGLTexture * tex = static_cast<const OpenGLTexture*>(mat->DiffuseTex.get());
		glBindTexture(GL_TEXTURE_2D, tex->TextureID);	
	}
	glUniform3fv(glGetUniformLocation(m_Program.GetProgramID(), "ambientIntensity"), 1, mat->Ambient);
	glUniform3fv(glGetUniformLocation(m_Program.GetProgramID(), "diffuseIntensity"), 1, mat->Diffuse);
	glBindSampler(0, m_LinearSampler);
	
}

void OpenGLRenderer::InitializeShaders()
{
	auto & shaderMan = m_Game->GetShaderManager();

	m_Program.Load(std::static_pointer_cast<const OpenGLShader>(shaderMan.Cache("shaders/vertex.vert")), 
					std::static_pointer_cast<const OpenGLShader>(shaderMan.Cache("shaders/pixel.frag")));
	m_ShadowmapProgram.Load(std::static_pointer_cast<const OpenGLShader>(shaderMan.Cache("shaders/shadow.vert")),
		std::static_pointer_cast<const OpenGLShader>(shaderMan.Cache("shaders/shadow.frag")));

	_ASSERT(m_Program.GetProgramID() != 0);
	_ASSERT(m_ShadowmapProgram.GetProgramID() != 0);
}

void OpenGLRenderer::InitializeSampler()
{
	m_LinearSampler = 0;
	glGenSamplers(1, &m_LinearSampler);
	glSamplerParameteri(m_LinearSampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glSamplerParameteri(m_LinearSampler, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glSamplerParameteri(m_LinearSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(m_LinearSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glSamplerParameterf(m_LinearSampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, 16.0f);
}

void OpenGLRenderer::InitializeBaseTexture()
{
	const int TEXTURE_SIZE = 4;
	const int TABLE_SIZE = 4 * TEXTURE_SIZE * TEXTURE_SIZE;
	GLubyte data[TABLE_SIZE];
	
	for (int i = 0; i < TABLE_SIZE; i++)
		data[i] = 255;

	glGenTextures(1, &m_BaseTexture);
	glBindTexture(GL_TEXTURE_2D, m_BaseTexture);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, TEXTURE_SIZE, TEXTURE_SIZE);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, TEXTURE_SIZE, TEXTURE_SIZE, GL_RGBA8, GL_UNSIGNED_BYTE, data);
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEXTURE_SIZE, TEXTURE_SIZE, 0, GL_UNSIGNED_BYTE, GL_RGBA, data);

}

Vector OpenGLRenderer::ConverToOpenGL(const Vector & vec) const
{
	return Vector(-vec.y, vec.z, -vec.x);
}

Angle OpenGLRenderer::ConverToOpenGL(const Angle & ang) const
{
	return Angle(ang.y, ang.z, -ang.x);
}