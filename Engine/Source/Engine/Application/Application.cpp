#include "Application.h"

namespace Okay
{
	Application::Application(std::string_view windowTitle, uint32_t windowWidth, uint32_t windowHeight)
	{
		glfwInitHint(GLFW_CLIENT_API, GLFW_NO_API);
		bool glInit = glfwInit();
		OKAY_ASSERT(glInit);

		m_window.initiate(windowTitle, windowWidth, windowHeight);
		m_renderer.initialize(m_window);
	}

	Application::~Application()
	{
		m_window.shutdown();
		m_renderer.shutdown();
	}

	void Application::run()
	{
		Timer frameTimer;

		m_renderer.updateChunkData(m_world.getChunkConst());

		while (m_window.isOpen())
		{
			TimeStep timeStep = frameTimer.measure();
			frameTimer.reset();

			m_window.processMessages();

			onUpdate(timeStep);

			m_renderer.render(m_world);
		}
	}
}
