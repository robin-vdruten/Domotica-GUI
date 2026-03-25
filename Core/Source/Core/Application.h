#pragma once

#include "Layer.h"
#include "Window.h"
#include "Image.h"

#include <glm/glm.hpp>

#include <string>
#include <memory>
#include <vector>
#include <set>
#include <queue>
#include <mutex>
#include <functional>

namespace Core {

	class ImGuiLayer;

	struct ApplicationSpecification
	{
		std::string Name = "Application";
		bool UseDockspace = true;
		WindowSpecification WindowSpec;
	};

	class Application
	{
	public:
		Application(const ApplicationSpecification& specification = ApplicationSpecification());
		~Application();

		void Run();
		void Stop();

		template<typename TLayer>
		requires(std::is_base_of_v<Layer, TLayer>)
		void PushLayer()
		{
			m_LayerStack.push_back(std::make_unique<TLayer>());
		}

		template<typename TLayer>
		requires(std::is_base_of_v<Layer, TLayer>)
		void PopLayer()
		{
			for (auto it = m_LayerStack.begin(); it != m_LayerStack.end(); ++it)
			{
				if (dynamic_cast<TLayer*>(it->get()) != nullptr)
				{
					m_LayerStack.erase(it);
					return;
				}
			}
		}

		template<typename TLayer>
		requires(std::is_base_of_v<Layer, TLayer>)
		TLayer* GetLayer()
		{
			for (const auto& layer : m_LayerStack)
			{
				if (auto* casted = dynamic_cast<TLayer*>(layer.get()))
					return casted;
			}
			return nullptr;
		}

		glm::vec2 GetFramebufferSize() const;

		Window* GetWindow() const { return m_Window.get(); }
		ApplicationSpecification& GetSpecification() { return m_Specification; }
		std::vector<std::unique_ptr<Layer>>& GetLayerStack() { return m_LayerStack; }

		bool IsRunning() const { return m_Running; }

		static Application& Get();

		static float GetTime();

		bool IsMaximized() const;

		template<typename Func>
		void QueueEvent(Func&& func)
		{
			m_EventQueue.push(func);
		}
	private:
		void UI_DrawTitlebar(float& outTitlebarHeight);
	private:
		ApplicationSpecification m_Specification;
		std::shared_ptr<Window> m_Window;
		bool m_Running = false;

		bool m_TitleBarHovered = false;

		std::shared_ptr<ImGuiLayer> m_ImGuiLayer;
		std::vector<std::unique_ptr<Layer>> m_LayerStack;

		std::mutex m_EventQueueMutex;
		std::queue<std::function<void()>> m_EventQueue;


		// TODO: MOVE OUT OF APPLICAITON LFIE TIME
		std::shared_ptr<Image> m_IconClose;
		std::shared_ptr<Image> m_IconMinimize;
		std::shared_ptr<Image> m_IconMaximize;
		std::shared_ptr<Image> m_IconRestore;
	};

}