#pragma once

#include "glm/glm.hpp"

#include <DirectXMath.h>
#include <DirectXCollision.h>

namespace Okay
{
    struct Camera;

    namespace Collision
    {
        typedef DirectX::BoundingFrustum Frustum;
        typedef DirectX::BoundingBox AABB;

        Frustum createFrustumFromCamera(const Camera& camera);
        AABB createAABB(const glm::vec3& center, const glm::vec3& extents);

        bool frustumAABB(const Frustum& frustum, const AABB& aabb);
    }
}
