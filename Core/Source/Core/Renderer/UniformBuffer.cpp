#include "UniformBuffer.h"

#include "Platform/OpenGL/OpenGLUniformBuffer.h"

namespace Renderer {

	std::shared_ptr<UniformBuffer> UniformBuffer::Create(uint32_t size, uint32_t binding)
	{
		return std::make_shared<OpenGLUniformBuffer>(size, binding);
	}

}
