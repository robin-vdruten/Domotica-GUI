#include "LoadingLayer.h"


#include "Core/Application.h"

#include "Core/Renderer/Renderer.h"
#include "Core/Renderer/Shader.h"
#include "Core/Renderer/RenderCommand.h"

#include <glm/glm.hpp>

#include <imgui.h>
#include <implot.h>

#include "MainLayer.h"

#include <print>

LoadingLayer::LoadingLayer()
{
	m_Shader = Renderer::Shader::Create("Shaders/loading.glsl");
	m_VertexArray = Renderer::VertexArray::Create();

	struct Vertex
	{
		glm::vec2 Position;
		glm::vec2 TexCoord;
	};

	Vertex vertices[] = {
		{ {-1.0f, -1.0f }, { 0.0f, 0.0f } },  // Bottom-left
		{ { 3.0f, -1.0f }, { 2.0f, 0.0f } },  // Bottom-right
		{ {-1.0f,  3.0f }, { 0.0f, 2.0f } }   // Top-left
	};

	uint32_t indices[] = { 0, 1, 2 };

	auto vertexBuffer = Renderer::VertexBuffer::Create((float*)(vertices), sizeof(vertices));
	vertexBuffer->SetLayout({
		{ Renderer::ShaderDataType::Float2, "a_Position" },
		{ Renderer::ShaderDataType::Float2, "a_TexCoord" }
		});
	m_VertexArray->AddVertexBuffer(vertexBuffer);

	auto indexBuffer = Renderer::IndexBuffer::Create(indices, 3);
	m_VertexArray->SetIndexBuffer(indexBuffer);


	Renderer::TextureSpecification spec;
	spec.Width = 1920;
	spec.Height = 1080;
	spec.Format = Renderer::ImageFormat::RGBA8;
	spec.GenerateMips = false;
	m_RenderTexture = Renderer::Texture2D::Create(spec);

	glCreateFramebuffers(1, &m_Framebuffer);
	glNamedFramebufferTexture(m_Framebuffer, GL_COLOR_ATTACHMENT0, m_RenderTexture->GetRendererID(), 0);
}

LoadingLayer::~LoadingLayer()
{
	if (m_Framebuffer != 0)
		glDeleteFramebuffers(1, &m_Framebuffer);
}

void LoadingLayer::OnUpdate(float ts)
{
	if (MainLayer::s_Loaded)
	{
		Core::Application::Get().QueueEvent([]() {
			Core::Application::Get().PopLayer<LoadingLayer>();
		});
	}
}


void LoadingLayer::OnRender()
{
	ImGuiWindowFlags windowFlags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoNavFocus;

	ImGui::Begin("Loading", nullptr, windowFlags);

	ImVec2 viewportSize = ImGui::GetContentRegionAvail();

	if (viewportSize.x > 0 && viewportSize.y > 0 &&
		(static_cast<uint32_t>(viewportSize.x) != m_RenderTexture->GetWidth() ||
			static_cast<uint32_t>(viewportSize.y) != m_RenderTexture->GetHeight()))
	{
		if (m_Framebuffer != 0)
			glDeleteFramebuffers(1, &m_Framebuffer);

		Renderer::TextureSpecification spec;
		spec.Width = static_cast<uint32_t>(viewportSize.x);
		spec.Height = static_cast<uint32_t>(viewportSize.y);
		spec.Format = Renderer::ImageFormat::RGBA8;
		spec.GenerateMips = false;
		m_RenderTexture = Renderer::Texture2D::Create(spec);

		glCreateFramebuffers(1, &m_Framebuffer);
		glNamedFramebufferTexture(m_Framebuffer, GL_COLOR_ATTACHMENT0, m_RenderTexture->GetRendererID(), 0);
	}
	if (m_Framebuffer != 0 && m_RenderTexture)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_Framebuffer);

		GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, drawBuffers);

		Renderer::RenderCommand::SetViewport(
			0, 0,
			m_RenderTexture->GetWidth(),
			m_RenderTexture->GetHeight()
		);

		m_Shader->Bind();
		m_Shader->SetFloat("u_Time", Core::Application::GetTime());
		m_Shader->SetFloat2("u_Resolution", {
			(float)m_RenderTexture->GetWidth(),
			(float)m_RenderTexture->GetHeight()
			});

		m_VertexArray->Bind();
		glDrawArrays(GL_TRIANGLES, 0, 3);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		auto fbSize = Core::Application::Get().GetFramebufferSize();
		Renderer::RenderCommand::SetViewport(
			0, 0,
			(uint32_t)fbSize.x,
			(uint32_t)fbSize.y
		);

		ImGui::Image(
			(ImTextureID)(uintptr_t)m_RenderTexture->GetRendererID(),
			{ (float)m_RenderTexture->GetWidth(), (float)m_RenderTexture->GetHeight() },
			{ 0, 1 }, { 1, 0 }
		);

		m_Shader->Unbind();
	}


	ImGui::End();
}