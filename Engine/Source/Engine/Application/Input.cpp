#include "Input.h"
#include "Window.h"

#include "ImguiHelper.h"

namespace Okay
{
	std::unordered_map<Key, bool> Input::s_keysLastFrame;
	std::unordered_map<Key, bool> Input::s_keysCurrentFrame;

	std::unordered_map<MouseButton, bool> Input::s_mouseButtonsLastFrame;
	std::unordered_map<MouseButton, bool> Input::s_mouseButtonsCurrentFrame;

	glm::vec2 Input::s_mousePosLastFrame;
	glm::vec2 Input::s_mousePosCurrentFrame;

	float Input::s_scrollDelta = 0.f;

	Window* Input::s_pWindow = nullptr;


	bool Input::isKeyPressed(Key keyCode)
	{
		return !s_keysLastFrame[keyCode] && s_keysCurrentFrame[keyCode];
	}

	bool Input::isKeyDown(Key keyCode)
	{
		return s_keysCurrentFrame[keyCode];
	}

	bool Input::isKeyReleased(Key keyCode)
	{
		return s_keysLastFrame[keyCode] && !s_keysCurrentFrame[keyCode];
	}

	bool Input::isMouseButtonPressed(MouseButton buttonCode)
	{
		return !s_mouseButtonsLastFrame[buttonCode] && s_mouseButtonsCurrentFrame[buttonCode];
	}

	bool Input::isMouseButtonDown(MouseButton buttonCode)
	{
		return s_mouseButtonsCurrentFrame[buttonCode];
	}

	bool Input::isMouseButtonReleased(MouseButton buttonCode)
	{
		return s_mouseButtonsLastFrame[buttonCode] && !s_mouseButtonsCurrentFrame[buttonCode];
	}

	void Input::setMouseMode(MouseMode mode)
	{
		Input::s_pWindow->setInputMode(mode);
		imguiToggleMouse(mode == MouseMode::FREE);
	}

	MouseMode Input::getMouseMode()
	{
		return Input::s_pWindow->getInputMode();
	}

	glm::vec2 Input::getMouseDelta()
	{
		return s_mousePosCurrentFrame - s_mousePosLastFrame;
	}

	float Input::getScrollDelta()
	{
		return s_scrollDelta;
	}

	void Input::preInputHandling()
	{
		s_keysLastFrame = s_keysCurrentFrame;
		s_mouseButtonsLastFrame = s_mouseButtonsCurrentFrame;

		s_mousePosLastFrame = s_mousePosCurrentFrame;
		s_scrollDelta = 0.f;
	}

	void Input::setKeyDown(Key keyCode)
	{
		s_keysCurrentFrame[keyCode] = true;
	}

	void Input::setKeyUp(Key keyCode)
	{
		s_keysCurrentFrame[keyCode] = false;
	}

	void Input::setMouseButtonDown(MouseButton buttonCode)
	{
		s_mouseButtonsCurrentFrame[buttonCode] = true;
	}

	void Input::setMouseButtonUp(MouseButton buttonCode)
	{
		s_mouseButtonsCurrentFrame[buttonCode] = false;
	}

	void Input::setMousePosition(glm::vec2 position)
	{
		s_mousePosCurrentFrame = position;
	}

	void Input::setScrollDelta(float delta)
	{
		s_scrollDelta = delta;
	}
}
