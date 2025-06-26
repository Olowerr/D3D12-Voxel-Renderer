#pragma once

#include "Engine/Okay.h"
#include "Camera.h"

namespace Okay
{
	class World
	{
	public:
		World();
		~World();

		Camera& getCamera();
		const Camera& getCameraConst() const;

	private:
		Camera m_camera;

	};
}
