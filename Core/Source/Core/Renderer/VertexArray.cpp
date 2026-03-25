#include "Core/Renderer/VertexArray.h"

#include "Platform/OpenGL/OpenGLVertexArray.h"

namespace Renderer {

	std::shared_ptr<VertexArray> VertexArray::Create()
	{
		return std::make_shared<OpenGLVertexArray>();
	}

}