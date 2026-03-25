#include "Core/Renderer/RenderCommand.h"

namespace Renderer {
	std::unique_ptr<RendererAPI> RenderCommand::s_RendererAPI = RendererAPI::Create();
}