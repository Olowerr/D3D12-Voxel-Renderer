#include "Application.h"
#include "Engine/Utilities/ThreadPool.h"

namespace Okay
{
	Application::Application(std::string_view windowTitle, uint32_t windowWidth, uint32_t windowHeight)
	{
		glfwInitHint(GLFW_CLIENT_API, GLFW_NO_API);
		bool glInit = glfwInit();
		OKAY_ASSERT(glInit);

		ThreadPool::initialize(std::thread::hardware_concurrency() / 2);

		m_window.initiate(windowTitle, windowWidth, windowHeight);
		m_renderer.initialize(m_window);
	}

	Application::~Application()
	{
		m_window.shutdown();
		m_renderer.shutdown();
		ThreadPool::shutdown();
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
			m_world.update();

			m_renderer.render(m_world);
		}
	}
}
