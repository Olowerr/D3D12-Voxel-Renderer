#include "World.h"

namespace Okay
{
	World::World()
	{
	}

	World::~World()
	{
	}

	Camera& World::getCamera()
	{
		return m_camera;
	}

	const Camera& World::getCameraConst() const
	{
		return m_camera;
	}
}
