#include "Application.h"

namespace Okay
{
	Application::Application(std::string_view windowTitle, uint32_t windowWidth, uint32_t windowHeight)
	{
		glfwInitHint(GLFW_CLIENT_API, GLFW_NO_API);

		bool glInit = glfwInit();
		OKAY_ASSERT(glInit);

		m_window.initiate(windowTitle, windowWidth, windowHeight);
	}

	Application::~Application()
	{
		m_window.shutdown();
	}

	void Application::run()
	{
		Timer frameTimer;

		while (m_window.isOpen())
		{
			TimeStep timeStep = frameTimer.measure();
			frameTimer.reset();

			m_window.processMessages();

			onUpdate(timeStep);
		}
	}
}
