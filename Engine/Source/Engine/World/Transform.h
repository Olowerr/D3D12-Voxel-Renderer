#pragma once

//
// Should probably find a better location for this file :3
//

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif

#include "glm/gtx/quaternion.hpp"

namespace Okay
{
	struct Transform
	{
		glm::vec3 position = glm::vec3(0.f);
		glm::vec3 rotation = glm::vec3(0.f);
		glm::vec3 scale = glm::vec3(1.f);

		inline glm::mat4 getMatrix() const
		{
			return glm::translate(glm::mat4(1.f), position) *
				glm::toMat4(glm::quat(glm::radians(rotation))) *
				glm::scale(glm::mat4(1.f), scale);
		}

		inline glm::mat4 getViewMatrix() const
		{
			return glm::lookAtLH(position, position + forwardVec(), upVec());
		}

		inline glm::vec3 forwardVec() const { return glm::normalize(getMatrix()[2]); }
		inline glm::vec3 rightVec() const { return glm::normalize(getMatrix()[0]); }
		inline glm::vec3 upVec() const { return glm::normalize(getMatrix()[1]); }
	};
}
