#include "Platform/OpenGL/OpenGLRendererAPI.h"

#include <glad/gl.h>

#include <iostream>
#define DEBUG_TEST false

namespace Renderer {
	void OpenGLMessageCallback(
		unsigned source,
		unsigned type,
		unsigned id,
		unsigned severity,
		int length,
		const char* message,
		const void* userParam)
	{
		const char* sourceStr;
		switch (source)
		{
		case GL_DEBUG_SOURCE_API:             sourceStr = "API"; break;
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   sourceStr = "Window System"; break;
		case GL_DEBUG_SOURCE_SHADER_COMPILER: sourceStr = "Shader Compiler"; break;
		case GL_DEBUG_SOURCE_THIRD_PARTY:     sourceStr = "Third Party"; break;
		case GL_DEBUG_SOURCE_APPLICATION:     sourceStr = "Application"; break;
		case GL_DEBUG_SOURCE_OTHER:           sourceStr = "Other"; break;
		default:                              sourceStr = "Unknown"; break;
		}

		const char* typeStr;
		switch (type)
		{
		case GL_DEBUG_TYPE_ERROR:               typeStr = "Error"; break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:typeStr = "Deprecated Behavior"; break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: typeStr = "Undefined Behavior"; break;
		case GL_DEBUG_TYPE_PORTABILITY:        typeStr = "Portability"; break;
		case GL_DEBUG_TYPE_PERFORMANCE:        typeStr = "Performance"; break;
		case GL_DEBUG_TYPE_MARKER:             typeStr = "Marker"; break;
		case GL_DEBUG_TYPE_PUSH_GROUP:         typeStr = "Push Group"; break;
		case GL_DEBUG_TYPE_POP_GROUP:          typeStr = "Pop Group"; break;
		case GL_DEBUG_TYPE_OTHER:              typeStr = "Other"; break;
		default:                               typeStr = "Unknown"; break;
		}

		const char* severityStr;
		switch (severity)
		{
		case GL_DEBUG_SEVERITY_HIGH:         severityStr = "High"; break;
		case GL_DEBUG_SEVERITY_MEDIUM:       severityStr = "Medium"; break;
		case GL_DEBUG_SEVERITY_LOW:          severityStr = "Low"; break;
		case GL_DEBUG_SEVERITY_NOTIFICATION: severityStr = "Notification"; break;
		default:                             severityStr = "Unknown"; break;
		}

		std::cerr << "[OpenGL][" << severityStr << "][" << sourceStr << "][" << typeStr
			<< "][" << id << "] " << message << "\n";
	}

	void OpenGLRendererAPI::Init()
	{

	#ifdef DEBUG_TEST
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback(OpenGLMessageCallback, nullptr);
		
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE);
	#endif

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_LINE_SMOOTH);
	}

	void OpenGLRendererAPI::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	{
		glViewport(x, y, width, height);
	}

	void OpenGLRendererAPI::SetClearColor(const glm::vec4& color)
	{
		glClearColor(color.r, color.g, color.b, color.a);
	}

	void OpenGLRendererAPI::Clear()
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	void OpenGLRendererAPI::DrawIndexedInstanced(const std::shared_ptr<VertexArray>& vertexArray, uint32_t instanceCount, uint32_t indexCount)
	{
		vertexArray->Bind();
		uint32_t count = indexCount ? indexCount : vertexArray->GetIndexBuffer()->GetCount();
		glDrawElementsInstanced(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr, instanceCount);
	}

	void OpenGLRendererAPI::DrawIndexed(const std::shared_ptr<VertexArray>& vertexArray, uint32_t indexCount)
	{
		vertexArray->Bind();
		uint32_t count = indexCount ? indexCount : vertexArray->GetIndexBuffer()->GetCount();
		glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
	}


	void OpenGLRendererAPI::DrawLines(const std::shared_ptr<VertexArray>& vertexArray, uint32_t vertexCount)
	{
		vertexArray->Bind();
		glDrawArrays(GL_LINES, 0, vertexCount);
	}

	void OpenGLRendererAPI::SetLineWidth(float width)
	{
		glLineWidth(width);
	}

}
