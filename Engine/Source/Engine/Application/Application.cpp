#include "Application.h"
#include "Engine/Utilities/ThreadPool.h"
#include "ImguiHelper.h"


namespace Okay
{
	Application::Application(std::string_view windowTitle, uint32_t windowWidth, uint32_t windowHeight)
	{
		glfwInitHint(GLFW_CLIENT_API, GLFW_NO_API);
		bool glInit = glfwInit();
		OKAY_ASSERT(glInit);

		m_window.initiate(windowTitle, windowWidth, windowHeight);
		m_renderer.initialize(m_window);

		m_world.initialize();

		m_camera.viewportDims = m_window.getWindowSize();
		m_window.registerResizeCallback([&](uint32_t width, uint32_t height)
			{
				m_camera.viewportDims.x = (float)width;
				m_camera.viewportDims.y = (float)height;
			});
	}

	Application::~Application()
	{
		m_window.shutdown();
		m_renderer.shutdown();
		m_world.shutdown();

		glfwTerminate();
	}

	void Application::run()
	{
		Timer frameTimer;

		while (m_window.isOpen())
		{
			TimeStep timeStep = frameTimer.measure();
			frameTimer.reset();

			m_window.processMessages();
			imguiNewFrame();

			onUpdate(timeStep);
			m_camera.frustum = Collision::createFrustumFromCamera(m_camera);

			m_world.update(m_camera);
			m_renderer.render(m_world, m_camera);
		}
	}
}
