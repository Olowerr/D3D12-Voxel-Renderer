#include "World.h"

namespace Okay
{
	static const int RENDER_DISTNACE = 8;

	World::World()
	{
		m_currentCamChunkCoord = glm::ivec2(0, 0);
	}

	World::~World()
	{
	}

	void World::update()
	{
		clearUpdatedChunks();
		m_currentCamChunkCoord = chunkIDToChunkCoord(blockCoordToChunkID(m_camera.transform.position));

		for (const auto& chunkIterator : m_chunks)
		{
			glm::ivec2 chunkCoord = chunkIDToChunkCoord(chunkIterator.first);
			if (!isChunkWithinRenderDistance(chunkCoord))
			{
				unloadChunk(chunkIterator.first);
				break;
			}
		}

		for (int chunkX = -RENDER_DISTNACE; chunkX <= RENDER_DISTNACE; chunkX++)
		{
			for (int chunkZ = -RENDER_DISTNACE; chunkZ <= RENDER_DISTNACE; chunkZ++)
			{
				glm::ivec2 chunkCoord = m_currentCamChunkCoord + glm::ivec2(chunkX, chunkZ);
				if (chunkCoord == m_currentCamChunkCoord)
				{
					generateChunkIfNotLoaded(chunkCoordToChunkID(m_currentCamChunkCoord));
					continue;
				}

				if (isChunkWithinRenderDistance(chunkCoord))
				{
					generateChunkIfNotLoaded(chunkCoordToChunkID(chunkCoord));
				}
			}
		}
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

	void World::clearUpdatedChunks()
	{
		m_newChunks.clear();
		m_removedChunks.clear();
	}

	bool World::isChunkWithinRenderDistance(const glm::ivec2& chunkCoord)
	{
		glm::vec2 chunkMiddle = glm::vec2(chunkCoord) + glm::vec2(CHUNK_WIDTH * 0.5f);
		glm::vec2 camChunkMiddle = glm::vec2(m_currentCamChunkCoord) + glm::vec2(CHUNK_WIDTH * 0.5f);

		return glm::length2(chunkMiddle - camChunkMiddle) < RENDER_DISTNACE * RENDER_DISTNACE;
	}

	const std::vector<ChunkID>& World::getNewChunks() const
	{
		return m_newChunks;
	}

	const std::vector<ChunkID>& World::getRemovedChunks() const
	{
		return m_removedChunks;
	}

	void World::generateChunk(ChunkID chunkID)
	{
		Chunk& chunk = getChunk(chunkID);

		for (uint32_t x = 0; x < CHUNK_WIDTH; x++)
		{
			for (uint32_t z = 0; z < CHUNK_WIDTH; z++)
			{
				float midHeight = 20.f;

				glm::ivec3 blockCoord = chunkBlockCoordToBlockCoord(chunkID, { x, 0, z });

				float sinHeight = glm::sin(blockCoord.x / 2.f) * 4.f;
				float cosHeight = glm::cos(blockCoord.z / 2.f) * 4.f;

				uint32_t columnHeight = (uint32_t)glm::round(midHeight + (sinHeight + cosHeight) * 0.5f);

				for (uint32_t y = 0; y < WORLD_HEIGHT; y++)
				{
					chunk.blocks[chunkBlockCoordToChunkBlockIdx({ x, y, z })] = y <= columnHeight;
				}
			}
		}

		m_newChunks.emplace_back(chunkID);
	}

	void World::generateChunkIfNotLoaded(ChunkID chunkID)
	{
		if (isChunkLoaded(chunkID))
			return;

		generateChunk(chunkID);
	}

	void World::unloadChunk(ChunkID chunkID)
	{
		m_chunks.erase(chunkID);
		m_removedChunks.emplace_back(chunkID);
	}

	void World::unloadChunkIfLoaded(ChunkID chunkID)
	{
		if (!isChunkLoaded(chunkID))
			return;

		unloadChunk(chunkID);
	}
}
