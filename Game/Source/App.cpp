#include "App.h"

#include "glm/common.hpp"

using namespace Okay;

App::App()
	:Application("D3D12 Voxel Renderer", 1600, 900)
{
}

void App::onUpdate(TimeStep dt)
{
	updateCamera(dt);

	static TimeStep passedTime = 0;
	static uint32_t numFrames = 0;

	passedTime += dt;
	numFrames++;

	if (passedTime >= 1.f)
	{
		TimeStep averageFps = passedTime / (float)numFrames;
		passedTime -= 1.f;
		numFrames = 0;

		static std::string windowTitle;
		windowTitle = "D3D12 Voxel Renderer | Fps: " + std::to_string((uint32_t)glm::round(1.f / averageFps));
		m_window.setWindowTitle(windowTitle);
	}
}

void App::updateCamera(TimeStep dt)
{
	Transform& camTransform = m_world.getCamera().transform;

	camTransform.rotation.x = 20.f;
	camTransform.rotation.y += 90.f * dt;

	camTransform.position = camTransform.forwardVec() * -5.f;
}
