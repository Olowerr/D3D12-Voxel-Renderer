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
	if (Input::isKeyPressed(Key::E))
	{
		MouseMode newMode = Input::getMouseMode() == MouseMode::LOCKED ? MouseMode::FREE : MouseMode::LOCKED;
		Input::setMouseMode(newMode);
	}

	if (Input::getMouseMode() == MouseMode::FREE)
	{
		return;
	}

	Transform& camTransform = m_world.getCamera().transform;
	float camMoveSpeed = 150.f;
	float camRotSpeed = 0.1f;

	// Movement
	float forwardDir = (float)Input::isKeyDown(Key::W) - (float)Input::isKeyDown(Key::S);
	float rightDir = (float)Input::isKeyDown(Key::D) - (float)Input::isKeyDown(Key::A);
	float upDir = (float)Input::isKeyDown(Key::SPACE) - (float)Input::isKeyDown(Key::L_SHIFT);

	glm::vec3 moveDir = glm::vec3(0.f);

	if (forwardDir)
	{
		moveDir += camTransform.forwardVec() * forwardDir;
	}
	if (rightDir)
	{
		moveDir += camTransform.rightVec() * rightDir;
	}
	if (upDir)
	{
		moveDir += camTransform.upVec() * upDir;
	}

	if (forwardDir || rightDir || upDir)
	{
		camTransform.position += glm::normalize(moveDir) * camMoveSpeed * dt;
	}


	// Rotation
	glm::vec2 mouseDelta = Input::getMouseDelta();
	camTransform.rotation.y += mouseDelta.x * camRotSpeed;
	camTransform.rotation.x += mouseDelta.y * camRotSpeed;
}
