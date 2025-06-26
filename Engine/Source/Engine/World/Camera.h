#pragma once

#include "Transform.h"

namespace Okay
{
	struct Camera
	{
		Transform transform = {};
		float fov = 90.f;
		float speed = 5.f;

		float nearZ = 0.01f;
		float farZ = 1000.f;

		inline glm::mat4 getProjectionMatrix(float width, float height) const
		{
			return glm::perspectiveFovLH_ZO(glm::radians(fov), width, height, nearZ, farZ);
		}
	};
}
