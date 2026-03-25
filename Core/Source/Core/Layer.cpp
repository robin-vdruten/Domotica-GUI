#include "Layer.h"

#include "Application.h"

namespace Core {

	void Layer::QeueueTransition(std::unique_ptr<Layer> toLayer)
	{
		// TODO: do this differently

		auto& stack = Application::Get().GetLayerStack();
		for (auto& layer : stack)
		{
			if (layer.get() == this)
			{
				layer = std::move(toLayer);
				break;
			}
		}
	}
}