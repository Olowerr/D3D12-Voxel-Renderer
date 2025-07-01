#include "World.h"

namespace Okay
{
	World::World()
	{
		for (uint32_t i = 0; i < MAX_BLOCKS_IN_CHUNK; i++)
		{
			glm::uvec3 chunkCoord = Chunk::blockIdxToChunkCoord(i);
			m_testChunk.blocks[i] = i % 2;
		}
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
