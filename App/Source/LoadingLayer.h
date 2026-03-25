#pragma once

#include <stdint.h>
#include <vector>

#include "Core/Layer.h"
#include "Core/Renderer/Renderer2D.h"
#include "Core/Renderer/Shader.h"

#include <glad/gl.h>


class LoadingLayer : public Core::Layer
{
public:
	LoadingLayer();
	virtual ~LoadingLayer();

	virtual void OnUpdate(float ts) override;
	virtual void OnRender() override;
private:
	std::shared_ptr<Renderer::Shader> m_Shader;

	std::shared_ptr<Renderer::VertexArray> m_VertexArray;
	std::shared_ptr<Renderer::Texture2D> m_RenderTexture;
	GLuint m_Framebuffer = 0;
};