#include "Collision.h"
#include "Engine/World/Camera.h"

#include "glm/gtc/type_ptr.hpp"

namespace Okay
{
    namespace Collision
    {
        static DirectX::XMFLOAT3 glmToDXFloat3(const glm::vec3& vec)
        {
            return DirectX::XMFLOAT3(vec.x, vec.y, vec.z);
        }

        static DirectX::XMFLOAT4 glmQuatToDXFloat4(const glm::quat& quat)
        {
            return DirectX::XMFLOAT4(quat.x, quat.y, quat.z, quat.w);
        }

        Frustum createFrustumFromCamera(const Camera& camera)
        {
            glm::mat4 projectionMatrix = camera.getProjectionMatrix();
            DirectX::FXMMATRIX dxMatrix = DirectX::XMLoadFloat4x4((DirectX::XMFLOAT4X4*)glm::value_ptr(projectionMatrix));

            Frustum frustum = {};
            DirectX::BoundingFrustum::CreateFromMatrix(frustum, dxMatrix);
            frustum.Near = camera.nearZ;
            frustum.Far = camera.farZ;

            frustum.Origin = glmToDXFloat3(camera.transform.position);
            frustum.Orientation = glmQuatToDXFloat4(glm::quat(glm::radians(camera.transform.rotation)));

            return frustum;
        }

        AABB createAABB(const glm::vec3& center, const glm::vec3& extents)
        {
            DirectX::XMFLOAT3 dxCenter = glmToDXFloat3(center);
            DirectX::XMFLOAT3 dxExtents = glmToDXFloat3(extents);

            DirectX::BoundingBox aabb = DirectX::BoundingBox(dxCenter, dxExtents);
            return aabb;
        }

        bool frustumAABB(const Frustum& frustum, const AABB& aabb)
        {
            return frustum.Intersects(aabb);
        }
    }
}
