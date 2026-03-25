#include "Core/Renderer/Texture.h"

#include "Core/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLTexture.h"

namespace Renderer {

	std::shared_ptr<Texture2D> Texture2D::Create(const TextureSpecification& specification)
	{
		return std::make_shared<OpenGLTexture2D>(specification);
	}

	std::shared_ptr<Texture2D> Texture2D::Create(const std::string& path)
	{
		 return std::make_shared<OpenGLTexture2D>(path);
	}

}
