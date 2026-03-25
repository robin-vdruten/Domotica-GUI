#pragma once

#include "Event.h"

#include <concepts>
#include <memory>

namespace Core {

	class Layer
	{
	public:
		virtual ~Layer() = default;

		virtual void OnEvent(Event& event) {}

		virtual void OnUpdate(float ts) {}
		virtual void OnRender() {}

		template<std::derived_from<Layer> T, typename... Args>
		void TransitionTo(Args&&... args)
		{
			QeueueTransition(std::move(std::make_unique<T>(std::forward<Args>(args)...)));
		}	
	private:
		void QeueueTransition(std::unique_ptr<Layer> layer);
	};

}