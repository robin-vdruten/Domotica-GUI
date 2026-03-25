#include "Application.h"

#include "ImGuiLayer.h"
#include "Renderer/GLUtils.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include <assert.h>
#include <iostream>
#include <unordered_map>

#include "UI/UI.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/RenderCommand.h"

namespace Core {
#include "Embed/WindowImages.embed"
#include "Embed/Roboto-Regular.embed"
#include "Embed/Roboto-Bold.embed"
#include "Embed/Roboto-Italic.embed"

	static std::unordered_map<std::string, ImFont*> s_Fonts;

	static Application* s_Application = nullptr;

	static void GLFWErrorCallback(int error, const char* description)
	{
		std::cerr << "[GLFW Error]: " << description << std::endl;
	}

	Application::Application(const ApplicationSpecification& specification)
		: m_Specification(specification)
	{
		s_Application = this;

		m_ImGuiLayer = std::make_shared<ImGuiLayer>();

		glfwSetErrorCallback(GLFWErrorCallback);
		glfwInit();

		// Set window title to app name if empty
		if (m_Specification.WindowSpec.Title.empty())
			m_Specification.WindowSpec.Title = m_Specification.Name;

		m_Window = std::make_shared<Window>(m_Specification.WindowSpec);
		m_Window->Create();

		Renderer::Utils::InitOpenGLDebugMessageCallback();


		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

		ImPlot::CreateContext();

		ImGui::StyleColorsDark();

		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		m_ImGuiLayer->SetDarkThemeColors();

		ImGui_ImplGlfw_InitForOpenGL(m_Window->GetHandle(), true);
		ImGui_ImplOpenGL3_Init("#version 460");


		ImFontConfig fontConfig;
		fontConfig.FontDataOwnedByAtlas = false;
		ImFont* robotoFont = io.Fonts->AddFontFromMemoryTTF((void*)g_RobotoRegular, sizeof(g_RobotoRegular), 20.0f, &fontConfig);
		s_Fonts["Default"] = robotoFont;
		s_Fonts["Bold"] = io.Fonts->AddFontFromMemoryTTF((void*)g_RobotoBold, sizeof(g_RobotoBold), 20.0f, &fontConfig);
		s_Fonts["Italic"] = io.Fonts->AddFontFromMemoryTTF((void*)g_RobotoItalic, sizeof(g_RobotoItalic), 20.0f, &fontConfig);
		io.FontDefault = robotoFont;

		// Upload Fonts

		{
			ImGui_ImplOpenGL3_DestroyFontsTexture();
			io.Fonts->Build();
			ImGui_ImplOpenGL3_CreateFontsTexture();
			GLuint fontTex = (GLuint)(intptr_t)io.Fonts->TexID;
		}

		{
			uint32_t w, h;
			void* data = Image::Decode(g_WindowMinimizeIcon, sizeof(g_WindowMinimizeIcon), w, h);
			m_IconMinimize = std::make_shared<Image>(w, h, ImageFormat::RGBA, data);
			free(data);
		}
		{
			uint32_t w, h;
			void* data = Image::Decode(g_WindowMaximizeIcon, sizeof(g_WindowMaximizeIcon), w, h);
			m_IconMaximize = std::make_shared<Image>(w, h, ImageFormat::RGBA, data);
			free(data);
		}
		{
			uint32_t w, h;
			void* data = Image::Decode(g_WindowRestoreIcon, sizeof(g_WindowRestoreIcon), w, h);
			m_IconRestore = std::make_shared<Image>(w, h, ImageFormat::RGBA, data);
			free(data);
		}
		{
			uint32_t w, h;
			void* data = Image::Decode(g_WindowCloseIcon, sizeof(g_WindowCloseIcon), w, h);
			m_IconClose = std::make_shared<Image>(w, h, ImageFormat::RGBA, data);
			free(data);
		}
	}

	Application::~Application()
	{
		ImPlot::DestroyContext();

		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		m_Window->Destroy();

		glfwTerminate();

		s_Application = nullptr;
	}

	void Application::Run()
	{
		m_Running = true;

		float lastTime = GetTime();

		while (m_Running)
		{
			glfwPollEvents();

			{
				std::scoped_lock<std::mutex> lock(m_EventQueueMutex);

				while (m_EventQueue.size() > 0)
				{
					auto& func = m_EventQueue.front();
					func();
					m_EventQueue.pop();
				}
			}

			if (m_Window->ShouldClose())
			{
				Stop();
				break;
			}

			float currentTime = GetTime();
			float timestep = glm::clamp(currentTime - lastTime, 0.001f, 0.1f);
			lastTime = currentTime;

			for (const std::unique_ptr<Layer>& layer : m_LayerStack)
				layer->OnUpdate(timestep);

			m_ImGuiLayer->Begin();

			if (m_Specification.UseDockspace)
			{
				ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;

				ImGuiViewport* viewport = ImGui::GetMainViewport();
				ImGui::SetNextWindowPos(viewport->Pos);
				ImGui::SetNextWindowSize(viewport->Size);
				ImGui::SetNextWindowViewport(viewport->ID);
				ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
				ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
				window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
				window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

				const bool isMaximized = IsMaximized();

				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, isMaximized ? ImVec2(6.0f, 6.0f) : ImVec2(1.0f, 1.0f));
				ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 3.0f);

				ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
				ImGui::Begin("DockSpaceWindow", nullptr, window_flags);
				ImGui::PopStyleColor(); // MenuBarBg
				ImGui::PopStyleVar(2);

				ImGui::PopStyleVar(2);

				{
					ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(50, 50, 50, 255));
					// Draw window border if the window is not maximized
					if (!isMaximized)
						UI::RenderWindowOuterBorders(ImGui::GetCurrentWindow());

					ImGui::PopStyleColor(); // ImGuiCol_Border
				}

				if (m_Specification.WindowSpec.CustomTitlebar)
				{
					float titleBarHeight;
					UI_DrawTitlebar(titleBarHeight);
					ImGui::SetCursorPosY(titleBarHeight);

				}

				ImGuiIO& io = ImGui::GetIO();
				ImGuiStyle& style = ImGui::GetStyle();
				float minWinSizeX = style.WindowMinSize.x;
				style.WindowMinSize.x = 370.0f;
				ImGui::DockSpace(ImGui::GetID("MyDockspace"));
				style.WindowMinSize.x = minWinSizeX;

				// NOTE: rendering can be done elsewhere (eg. render thread)
				for (const std::unique_ptr<Layer>& layer : m_LayerStack)
					layer->OnRender();

				ImGui::End();
			}
			else
			{
				// NOTE: rendering can be done elsewhere (eg. render thread)
				for (const std::unique_ptr<Layer>& layer : m_LayerStack)
					layer->OnRender();
			}

			m_ImGuiLayer->End();

			m_Window->Update();
		}
	}

	void Application::Stop()
	{
		m_Running = false;
	}

	glm::vec2 Application::GetFramebufferSize() const
	{
		return m_Window->GetFramebufferSize();
	}

	Application& Application::Get()
	{
		assert(s_Application);
		return *s_Application;
	}

	float Application::GetTime()
	{
		return (float)glfwGetTime();
	}

	bool Application::IsMaximized() const
	{
		return (bool)glfwGetWindowAttrib(m_Window->GetHandle(), GLFW_MAXIMIZED);
	}

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <windows.h>
#endif

	void Application::UI_DrawTitlebar(float& outTitlebarHeight)
	{
		const float titlebarHeight = 58.0f;
		const bool isMaximized = IsMaximized();
		float titlebarVerticalOffset = isMaximized ? -6.0f : 0.0f;
		const ImVec2 windowPadding = ImGui::GetCurrentWindow()->WindowPadding;

		ImGui::SetCursorPos(ImVec2(windowPadding.x, windowPadding.y + titlebarVerticalOffset));
		const ImVec2 titlebarMin = ImGui::GetCursorScreenPos();
		const ImVec2 titlebarMax = { ImGui::GetCursorScreenPos().x + ImGui::GetWindowWidth() - windowPadding.y * 2.0f,
									 ImGui::GetCursorScreenPos().y + titlebarHeight };
		auto* bgDrawList = ImGui::GetBackgroundDrawList();
		auto* fgDrawList = ImGui::GetForegroundDrawList();
		bgDrawList->AddRectFilled(titlebarMin, titlebarMax, UI::Colors::Theme::titlebar);

		// Layout constants
		const float buttonsAreaWidth = 94.0f;
		const float buttonWidth = 14.0f;
		const float buttonHeight = 14.0f;
		const float spacingMinToMax = 17.0f;
		const float spacingMaxToClose = 15.0f;
		const float trailingRightPadding = 18.0f;

		const float w = ImGui::GetContentRegionAvail().x;

		// Title bar drag area
		ImGui::SetCursorPos(ImVec2(windowPadding.x, windowPadding.y + titlebarVerticalOffset)); // Reset cursor pos
		ImGui::InvisibleButton("##titleBarDragZone", ImVec2(w - buttonsAreaWidth, titlebarHeight));

		m_TitleBarHovered = ImGui::IsItemHovered();
		if (isMaximized)
		{
			float windowMousePosY = ImGui::GetMousePos().y - ImGui::GetCursorScreenPos().y;
			if (windowMousePosY >= 0.0f && windowMousePosY <= 5.0f)
				m_TitleBarHovered = true;
		}

		// Double-click on drag zone toggles maximize/restore
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			Application::Get().QueueEvent([isMaximized, windowHandle = m_Window->GetHandle()]()
				{
					if (isMaximized)
						glfwRestoreWindow(windowHandle);
					else
						glfwMaximizeWindow(windowHandle);
				});
		}

		// Drag to move window
		{
			static bool s_SentNativeDrag = false;
			static bool s_DraggingManual = false;
			static ImVec2 s_MouseClickPos;
			static int s_WindowClickX = 0, s_WindowClickY = 0;

			GLFWwindow* windowHandle = m_Window ? m_Window->GetHandle() : nullptr;

			// Begin drag on mouse down within the drag zone
			if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				s_SentNativeDrag = false;
				s_DraggingManual = false;
				s_MouseClickPos = ImGui::GetMousePos();

				if (windowHandle)
					glfwGetWindowPos(windowHandle, &s_WindowClickX, &s_WindowClickY);
			}

			// While holding left mouse, perform window move
			if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
			{
#ifdef _WIN32
				// Use native OS drag once per drag action
				if (!s_SentNativeDrag && windowHandle)
				{
					HWND hwnd = glfwGetWin32Window(windowHandle);
					if (hwnd)
					{
						// If currently maximized, restore before starting drag so it can move
						if (IsMaximized())
							glfwRestoreWindow(windowHandle);

						ReleaseCapture();
						SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
						s_SentNativeDrag = true;
					}
				}
#else
				// Manual drag for non-Windows platforms
				if (windowHandle)
				{
					s_DraggingManual = true;
					ImVec2 mouse = ImGui::GetMousePos();
					int newX = s_WindowClickX + int(mouse.x - s_MouseClickPos.x);
					int newY = s_WindowClickY + int(mouse.y - s_MouseClickPos.y);
					glfwSetWindowPos(windowHandle, newX, newY);
				}
#endif
			}
			else
			{
				// End drag on mouse release
				if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
				{
					s_SentNativeDrag = false;
					s_DraggingManual = false;
				}
			}
		}

		// Centered Window title
		{
			ImVec2 currentCursorPos = ImGui::GetCursorPos();
			ImVec2 textSize = ImGui::CalcTextSize(m_Specification.Name.c_str());
			ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() * 0.5f - textSize.x * 0.5f, 2.0f + windowPadding.y + 6.0f));
			ImGui::Text("%s", m_Specification.Name.c_str());
			ImGui::SetCursorPos(currentCursorPos);
		}

		// Button colors
		const ImU32 buttonColN = UI::Colors::ColorWithMultipliedValue(UI::Colors::Theme::text, 0.9f);
		const ImU32 buttonColH = UI::Colors::ColorWithMultipliedValue(UI::Colors::Theme::text, 1.2f);
		const ImU32 buttonColP = UI::Colors::Theme::textDarker;

		// Anchor the button cluster to the right
		const float baseY = windowPadding.y + titlebarVerticalOffset + 8.0f;
		const float buttonsStartX = windowPadding.x + (w - buttonsAreaWidth);
		float x = buttonsStartX;

		// Minimize
		{
			ImGui::SetCursorPos(ImVec2(x, baseY));
			const int iconWidth = m_IconMinimize->GetWidth();
			const int iconHeight = m_IconMinimize->GetHeight();
			const float padY = (buttonHeight - (float)iconHeight) / 2.0f;

			if (ImGui::InvisibleButton("Minimize", ImVec2(buttonWidth, buttonHeight)))
			{
				if (m_Window->GetHandle())
				{
					Application::Get().QueueEvent([windowHandle = m_Window->GetHandle()]()
						{
							glfwIconifyWindow(windowHandle);
						});
				}
			}
			UI::DrawButtonImage(m_IconMinimize, buttonColN, buttonColH, buttonColP, UI::RectExpanded(UI::GetItemRect(), 0.0f, -padY));
			x += buttonWidth + spacingMinToMax;
		}

		// Maximize/Restore
		{
			ImGui::SetCursorPos(ImVec2(x, baseY));
			const bool maximized = IsMaximized();
			if (ImGui::InvisibleButton("Maximize", ImVec2(buttonWidth, buttonHeight)))
			{
				Application::Get().QueueEvent([maximized, windowHandle = m_Window->GetHandle()]()
					{
						if (maximized)
							glfwRestoreWindow(windowHandle);
						else
							glfwMaximizeWindow(windowHandle);
					});
			}
			UI::DrawButtonImage(maximized ? m_IconRestore : m_IconMaximize, buttonColN, buttonColH, buttonColP);
			x += buttonWidth + spacingMaxToClose;
		}

		// Close
		{
			ImGui::SetCursorPos(ImVec2(x, baseY));
			if (ImGui::InvisibleButton("Close", ImVec2(buttonWidth, buttonHeight)))
				Application::Get().Stop();

			UI::DrawButtonImage(m_IconClose, UI::Colors::Theme::text, UI::Colors::ColorWithMultipliedValue(UI::Colors::Theme::text, 1.4f), buttonColP);
			x += buttonWidth + trailingRightPadding;
		}

		outTitlebarHeight = titlebarHeight;
	}

}