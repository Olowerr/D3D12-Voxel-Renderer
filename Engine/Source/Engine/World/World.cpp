#include "World.h"

namespace Okay
{
	World::World()
	{
		for (int x = -1; x <= 1; x++)
		{
			for (int z = -1; z <= 1; z++)
			{
				generateChunk(glm::ivec2(x, z));
			}
		}
	}

	World::~World()
	{
	}

	bool World::isChunkBlockCoordOccupied(const glm::ivec3& blockCoord) const
	{
		if (blockCoord.y < 0 || blockCoord.y >= WORLD_HEIGHT)
			return false;

		ChunkID chunkID = blockCoordToChunkID(blockCoord);
		const Chunk* pChunk = tryGetChunk(chunkID);
		if (!pChunk)
			return false;

		glm::ivec3 chunkBlockCoord = blockCoordToChunkBlockCoord(blockCoord);
		uint32_t chunkBlockIdx = chunkBlockCoordToChunkBlockIdx(chunkBlockCoord);
		return pChunk->blocks[chunkBlockIdx] != 0;
	}

	Camera& World::getCamera()
	{
		return m_camera;
	}

	const Camera& World::getCameraConst() const
	{
		return m_camera;
	}

	Chunk& World::getChunk(ChunkID chunkID)
	{
		return m_chunks[chunkID];
	}

	const Chunk& World::getChunkConst(ChunkID chunkID) const
	{
		auto iterator = m_chunks.find(chunkID);
		OKAY_ASSERT(iterator != m_chunks.end()); // Temp?

		return iterator->second;
	}

	const Chunk* World::tryGetChunk(ChunkID chunkID) const
	{
		auto iterator = m_chunks.find(chunkID);
		if (iterator == m_chunks.end())
			return nullptr;

		return &iterator->second;
	}

	bool World::isChunkLoaded(ChunkID chunkID) const
	{
		return m_chunks.find(chunkID) != m_chunks.end();
	}

	void World::clearNewChunks()
	{
		m_newChunks.clear();
	}

	const std::vector<ChunkID>& World::getNewChunks() const
	{
		return m_newChunks;
	}

	void World::generateChunk(const glm::ivec2& chunkCoord)
	{
		ChunkID chunkID = chunkCoordToChunkID(chunkCoord);
		Chunk& chunk = getChunk(chunkID);

		for (uint32_t i = 0; i < MAX_BLOCKS_IN_CHUNK; i++)
		{
			chunk.blocks[i] = 1;// i % 2;
		}

		m_newChunks.emplace_back(chunkID);
	}
}
