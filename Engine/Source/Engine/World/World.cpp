#include "World.h"

namespace Okay
{
	World::World()
	{
		for (int x = -2; x <= 2; x++)
		{
			for (int z = -2; z <= 2; z++)
			{
				generateChunk(glm::ivec2(x, z));
			}
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

	Chunk& World::getChunk(ChunkID chunkId)
	{
		return m_chunks[chunkId];
	}

	const Chunk& World::getChunkConst(ChunkID chunkId) const
	{
		auto iterator = m_chunks.find(chunkId);
		OKAY_ASSERT(iterator != m_chunks.end()); // Temp?

		return iterator->second;
	}

	void World::clearNewChunks()
	{
		m_newChunks.clear();
	}

	const std::vector<ChunkID>& World::getNewChunks() const
	{
		return m_newChunks;
	}

	void World::generateChunk(const glm::ivec2& worldPos)
	{
		ChunkID chunkID = chunkPosToChunkID(worldPos);
		Chunk& chunk = getChunk(chunkID);

		for (uint32_t i = 0; i < MAX_BLOCKS_IN_CHUNK; i++)
		{
			chunk.blocks[i] = i % 2;
		}

		m_newChunks.emplace_back(chunkID);
	}
}
