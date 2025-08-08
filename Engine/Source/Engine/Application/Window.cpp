#include "Window.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3native.h"

namespace Okay
{
	void Window::initiate(std::string_view windowTitle, uint32_t windowWidth, uint32_t windowHeight)
	{
		m_pGlfwWindow = glfwCreateWindow((int)windowWidth, (int)windowHeight, windowTitle.data(), nullptr, nullptr);
		OKAY_ASSERT(m_pGlfwWindow);

		glfwSetWindowUserPointer(m_pGlfwWindow, this);
		Input::s_pWindow = this;

		// Place window in the center of the screen
		if (GLFWmonitor* pMonitor = glfwGetPrimaryMonitor())
		{
			int monitorWidth = -1, monitorHeight = -1;

			glfwGetMonitorWorkarea(pMonitor, nullptr, nullptr, &monitorWidth, &monitorHeight);
			glfwSetWindowPos(m_pGlfwWindow, int((monitorWidth - windowWidth) * 0.5f), int((monitorHeight - windowHeight) * 0.5f));
		}

		glfwSetKeyCallback(m_pGlfwWindow, [](GLFWwindow* pWindow, int key, int scancode, int action, int mods)
		{
			if (action == GLFW_PRESS)
			{
				Input::setKeyDown(Key(key));
			}
			else if (action == GLFW_RELEASE)
			{
				Input::setKeyUp(Key(key));
			}
		});

		glfwSetMouseButtonCallback(m_pGlfwWindow, [](GLFWwindow* pWindow, int button, int action, int mods)
		{
			if (action == GLFW_PRESS)
			{
				Input::setMouseButtonDown(MouseButton(button));
			}
			else if (action == GLFW_RELEASE)
			{
				Input::setMouseButtonUp(MouseButton(button));
			}
		});

		if (glfwRawMouseMotionSupported())
		{
			glfwSetInputMode(m_pGlfwWindow, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
		}

		glfwSetCursorPosCallback(m_pGlfwWindow, [](GLFWwindow* pWindow, double xPos, double yPos)
		{
			Input::setMousePosition(glm::vec2(xPos, yPos));
		});

		glfwSetScrollCallback(m_pGlfwWindow, [](GLFWwindow* pWindow, double xOffset, double yOffset)
		{
			Input::setScrollDelta((float)yOffset);
		});

		glfwSetFramebufferSizeCallback(m_pGlfwWindow, [](GLFWwindow* pGlfwWindow, int width, int height)
		{
			Window* pWindow = (Window*)glfwGetWindowUserPointer(pGlfwWindow);
			pWindow->onResize((uint32_t)width, (uint32_t)height);
		});

		glfwShowWindow(m_pGlfwWindow);
	}

	void Window::shutdown()
	{
		if (m_pGlfwWindow)
		{
			glfwDestroyWindow(m_pGlfwWindow);
			m_pGlfwWindow = nullptr;
		}
	}

	bool Window::isOpen() const
	{
		return !glfwWindowShouldClose(m_pGlfwWindow);
	}

	void Window::processMessages()
	{
		Input::preInputHandling();
		glfwPollEvents();
	}

	HWND Window::getHWND() const
	{
		return glfwGetWin32Window(m_pGlfwWindow);
	}

	GLFWwindow* Window::getGLFWWindow() const
	{
		return m_pGlfwWindow;
	}

	void Window::setWindowTitle(std::string_view newTitle)
	{
		glfwSetWindowTitle(m_pGlfwWindow, newTitle.data());
	}

	void Window::registerResizeCallback(std::function<void(uint32_t, uint32_t)> callback)
	{
		m_resizeCallbacks.emplace_back(callback);
	}

	glm::uvec2 Window::getWindowSize() const
	{
		glm::uvec2 size = glm::uvec2(0);
		glfwGetWindowSize(m_pGlfwWindow, (int*)&size.x, (int*)&size.y);
		return size;
	}

	void Window::setInputMode(MouseMode mode)
	{
		// For now mouse mode should not be changed to anything but these
		OKAY_ASSERT(mode == MouseMode::FREE || mode == MouseMode::LOCKED);

		glfwSetInputMode(m_pGlfwWindow, GLFW_CURSOR, (int)mode);
	}

	MouseMode Window::getInputMode()
	{
		return (MouseMode)glfwGetInputMode(m_pGlfwWindow, GLFW_CURSOR);
	}

	void Window::onResize(uint32_t width, uint32_t height)
	{
		for (const std::function<void(uint32_t, uint32_t)>& callback : m_resizeCallbacks)
		{
			callback(width, height);
		}
	}
}
