#pragma once

#include "Transform.h"
#include "Engine/Utilities/Collision.h"
#include "Engine/Okay.h"

namespace Okay
{
	struct Camera
	{
		Transform transform = {};
		Collision::Frustum frustum = {};

		float fov = 90.f;
		glm::vec2 viewportDims = glm::vec2(0.f);

		float nearZ = 0.1f;
		float farZ = 1000.f;

		inline glm::mat4 getProjectionMatrix() const
		{
			return glm::perspectiveFovLH_ZO(glm::radians(fov), viewportDims.x, viewportDims.y, nearZ, farZ);
		}
	};
}
