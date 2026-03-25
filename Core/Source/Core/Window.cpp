#include "Window.h"

#include <glad/gl.h>

#include <iostream>
#include <assert.h>

namespace Core {

	Window::Window(const WindowSpecification& specification)
		: m_Specification(specification)
	{
	}

	Window::~Window()
	{
		Destroy();
	}

	void Window::Create()
	{
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_DECORATED, m_Specification.CustomTitlebar ? GLFW_FALSE : GLFW_TRUE);

		GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
		const GLFWvidmode* videoMode = glfwGetVideoMode(primaryMonitor);

		int monitorX, monitorY;
		glfwGetMonitorPos(primaryMonitor, &monitorX, &monitorY);

		m_Handle = glfwCreateWindow(m_Specification.Width, m_Specification.Height,
			m_Specification.Title.c_str(), nullptr, nullptr);

		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
		m_ContextHandle = glfwCreateWindow(1, 1, "Context", NULL, m_Handle);
		glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

		if (!m_Handle || !m_ContextHandle)
		{
			std::cerr << "Failed to create GLFW window!\n";
			assert(false);
		}

		if (m_Specification.CenterWindow)
		{
			glfwSetWindowPos(m_Handle,
				monitorX + (videoMode->width - m_Specification.Width) / 2,
				monitorY + (videoMode->height - m_Specification.Height) / 2);

			glfwSetWindowAttrib(m_Handle, GLFW_RESIZABLE, m_Specification.WindowResizeable ? GLFW_TRUE : GLFW_FALSE);
		}

		glfwSetWindowUserPointer(m_Handle, this);

		glfwMakeContextCurrent(m_Handle);
		gladLoadGL(glfwGetProcAddress);

		glfwSwapInterval(m_Specification.VSync ? 1 : 0);
	}

	void Window::Destroy()
	{
		if (m_ContextHandle)
			glfwDestroyWindow(m_ContextHandle);

		if (m_Handle)
			glfwDestroyWindow(m_Handle);

		m_Handle = nullptr;
		m_ContextHandle = nullptr;
	}

	void Window::Update()
	{
		glfwSwapBuffers(m_Handle);
	}

	glm::vec2 Window::GetFramebufferSize()
	{
		int width, height;
		glfwGetFramebufferSize(m_Handle, &width, &height);
		return { width, height };
	}

	bool Window::ShouldClose() const
	{
		return glfwWindowShouldClose(m_Handle) != 0;
	}

}