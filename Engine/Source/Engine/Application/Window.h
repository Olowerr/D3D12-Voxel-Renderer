#pragma once
#include "Engine/Okay.h"
#include "Input.h"

#include "GLFW/glfw3.h"

#include <windows.h>
#include <string_view>
#include <functional>

namespace Okay
{
	class Window
	{
	public:
		friend class Input;

		Window() = default;
		virtual ~Window();

		void initiate(std::string_view windowTitle, uint32_t windowWidth, uint32_t windowHeight);
		void shutdown();

		bool isOpen() const;
		void processMessages();

		HWND getHWND() const;
		GLFWwindow* getGLFWWindow() const;

		void setWindowTitle(std::string_view newTitle);
		void registerResizeCallback(std::function<void(uint32_t, uint32_t)> callback);

		glm::uvec2 getWindowSize() const;

	private:
		void setInputMode(MouseMode mode);
		MouseMode getInputMode();

		void onResize(uint32_t width, uint32_t height);

	private:
		GLFWwindow* m_pGlfwWindow = nullptr;
		std::vector<std::function<void(uint32_t, uint32_t)>> m_resizeCallbacks;
	};
}
