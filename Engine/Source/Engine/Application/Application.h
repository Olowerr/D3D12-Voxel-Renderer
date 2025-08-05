#pragma once

#include "Window.h"
#include "Engine/World/Camera.h"
#include "Engine/D3D12/Renderer.h"
#include "Time.h"
#include "Engine/World/World.h"

namespace Okay
{
	class Application
	{
	public:
		Application(std::string_view windowTitle, uint32_t windowWidth, uint32_t windowHeight);
		virtual ~Application();

		void run();

	protected:
		virtual void onUpdate(TimeStep dt) = 0;

	protected:
		Window m_window;
		Camera m_camera;
		World m_world;
		Renderer m_renderer;

	};
}
