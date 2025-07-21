#include "World.h"
#include <shared_mutex>
#include <atomic>

#include "../Application/Time.h"

namespace Okay
{
	static const int RENDER_DISTNACE = 8;
	static std::shared_mutex mutis;

	World::World()
	{
	}

	World::~World()
	{
	}

	void World::update()
	{
		std::unique_lock lock(mutis);

		clearUpdatedChunks();
		m_currentCamChunkCoord = chunkIDToChunkCoord(blockCoordToChunkID(m_camera.transform.position));

		auto chunkIterator = m_chunks.begin();
		while (chunkIterator != m_chunks.end())
		{
			ChunkID chunkID = chunkIterator->first;
			glm::ivec2 chunkCoord = chunkIDToChunkCoord(chunkID);

			if (chunkCoord == m_currentCamChunkCoord)
			{
				++chunkIterator;
				continue;
			}

			if (isChunkWithinRenderDistance(chunkCoord))
			{
				++chunkIterator;
				continue;
			}

			chunkIterator = m_chunks.erase(chunkIterator);
			m_removedChunks.emplace_back(chunkID);
		}

		for (int chunkX = -RENDER_DISTNACE; chunkX <= RENDER_DISTNACE; chunkX++)
		{
			for (int chunkZ = -RENDER_DISTNACE; chunkZ <= RENDER_DISTNACE; chunkZ++)
			{
				glm::ivec2 chunkCoord = m_currentCamChunkCoord + glm::ivec2(chunkX, chunkZ);
				ChunkID chunkID = chunkCoordToChunkID(chunkCoord);

				if (chunkCoord == m_currentCamChunkCoord)
				{
					if (isChunkLoaded(chunkID))
						continue;

					generateChunk(chunkID);
					m_newlyLoadedChunk = chunkID;
					return;
				}

				if (isChunkLoaded(chunkID))
					continue;

				if (!isChunkWithinRenderDistance(chunkCoord))
					continue;

				generateChunk(chunkID);
				m_newlyLoadedChunk = chunkID;
				return;
			}
		}
	}

	bool World::isChunkBlockCoordOccupied(const glm::ivec3& blockCoord) const
	{
		if (blockCoord.y < 0 || blockCoord.y >= WORLD_HEIGHT)
			return false;

		ChunkID chunkID = blockCoordToChunkID(blockCoord);

		std::shared_lock lock(mutis);
		const Chunk* pChunk = tryGetChunk(chunkID);
		if (!pChunk)
			return false;

		glm::ivec3 chunkBlockCoord = blockCoordToChunkBlockCoord(blockCoord);
		uint32_t chunkBlockIdx = chunkBlockCoordToChunkBlockIdx(chunkBlockCoord);

		return pChunk->blocks[chunkBlockIdx] != 0;
	}

	uint8_t World::tryGetBlock(ChunkID chunkID, uint32_t blockIdx) const
	{
		std::shared_lock lock(mutis);
		const Chunk* pChunk = tryGetChunk(chunkID);
		if (!pChunk)
			return INVALID_UINT8;
	
		return pChunk->blocks[blockIdx];
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
		return iterator == m_chunks.end() ? nullptr : &iterator->second;
	}

	bool World::isChunkLoaded(ChunkID chunkID) const
	{
		return m_chunks.find(chunkID) != m_chunks.end();
	}

	void World::clearUpdatedChunks()
	{
		m_newlyLoadedChunk = INVALID_CHUNK_ID;
		m_removedChunks.clear();
	}

	bool World::isChunkWithinRenderDistance(const glm::ivec2& chunkCoord) const
	{
		glm::vec2 chunkMiddle = glm::vec2(chunkCoord);
		glm::vec2 camChunkMiddle = glm::vec2(m_currentCamChunkCoord);
		return glm::length2(chunkMiddle - camChunkMiddle) < RENDER_DISTNACE * RENDER_DISTNACE; // wrong lmao
	}

	ChunkID World::getNewlyLoadedChunk() const
	{
		return m_newlyLoadedChunk;
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
				//glm::ivec3 blockCoord = chunkBlockCoordToBlockCoord(chunkID, { x, 0, z });

				float height1 = glm::sin((x / 15.f) * glm::pi<float>()) * 16.f;
				float height2 = glm::sin((z / 15.f) * glm::pi<float>()) * 16.f;

				height1 = glm::abs(height1);
				height2 = glm::abs(height2);

				uint32_t columnHeight = (uint32_t)glm::floor(20.f + (height1 + height2) * 0.5f);

				for (uint32_t y = 0; y < columnHeight; y++)
				{
					chunk.blocks[chunkBlockCoordToChunkBlockIdx({ x, y, z })] = 1;
				}
			}
		}
	}
}
