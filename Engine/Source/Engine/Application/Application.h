#pragma once

#include "Engine/D3D12/Renderer.h"
#include "Time.h"

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

	private:
		Renderer m_renderer;
	};
}
