#include "World.h"

namespace Okay
{
	World::World()
	{
		for (uint32_t& block : m_testChunk.blocks)
			block = 1;
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

	const Chunk& World::getChunkConst() const
	{
		return m_testChunk;
	}
}
