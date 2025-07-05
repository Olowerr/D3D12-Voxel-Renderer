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
	static float camXRotDir = 1.f;

	Transform& camTransform = m_world.getCamera().transform;

	camTransform.rotation.y += 90.f * dt;

	camTransform.rotation.x += 30.f * camXRotDir * dt;
	if (camTransform.rotation.x > 45.f || camTransform.rotation.x < -45.f)
	{
		camXRotDir *= -1.f;
		camTransform.rotation.x = glm::clamp(camTransform.rotation.x, -45.f, 45.f);
	}

	camTransform.position = camTransform.forwardVec() * -100.f;
	camTransform.position += glm::vec3(CHUNK_WIDTH * 0.5f, WORLD_HEIGHT * 0.5f, CHUNK_WIDTH * 0.5f);

}
