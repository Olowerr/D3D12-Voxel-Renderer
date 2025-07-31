#include "World.h"
#include "../Application/Time.h"
#include "../Application/Input.h"
#include "Engine/Utilities/ThreadPool.h"

#include <shared_mutex>
#include <chrono>

namespace Okay
{
	static const uint32_t RENDER_DISTNACE = 16;
	static std::shared_mutex mutis;

	World::World()
	{
	}

	World::~World()
	{
	}

	void World::update()
	{
		clearUpdatedChunks();
		m_currentCamChunkCoord = chunkIDToChunkCoord(blockCoordToChunkID(m_camera.transform.position));

		std::unique_lock lock(mutis);
		unloadDistantChunks();
		processLoadingChunks();
		tryLoadRenderEligableChunks();
	}

	BlockType World::getBlockAtBlockCoord(const glm::ivec3& blockCoord) const
	{
		if (blockCoord.y < 0 || blockCoord.y >= WORLD_HEIGHT)
			return BlockType::AIR;

		ChunkID chunkID = blockCoordToChunkID(blockCoord);

		glm::ivec3 chunkBlockCoord = blockCoordToChunkBlockCoord(blockCoord);
		uint32_t chunkBlockIdx = chunkBlockCoordToChunkBlockIdx(chunkBlockCoord);

		return tryGetBlock(chunkID, chunkBlockIdx);
	}

	BlockType World::tryGetBlock(ChunkID chunkID, uint32_t blockIdx) const
	{
		std::shared_lock lock(mutis);
		const Chunk* pChunk = tryGetChunk(chunkID);
		return pChunk ? pChunk->blocks[blockIdx] : BlockType::INVALID;
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
		return m_loadedChunks[chunkID];
	}

	const Chunk& World::getChunkConst(ChunkID chunkID) const
	{
		auto iterator = m_loadedChunks.find(chunkID);
		OKAY_ASSERT(iterator != m_loadedChunks.end());
		return iterator->second;
	}

	const Chunk* World::tryGetChunk(ChunkID chunkID) const
	{
		auto iterator = m_loadedChunks.find(chunkID);
		return iterator == m_loadedChunks.end() ? nullptr : &iterator->second;
	}

	bool World::isChunkLoaded(ChunkID chunkID) const
	{
		return m_loadedChunks.contains(chunkID);
	}

	void World::clearUpdatedChunks()
	{
		m_addedChunks.clear();
		m_removedChunks.clear();
	}

	void World::unloadDistantChunks()
	{
		auto chunkIterator = m_loadedChunks.begin();
		while (chunkIterator != m_loadedChunks.end())
		{
			ChunkID chunkID = chunkIterator->first;
			if (isChunkWithinRenderDistance(chunkID))
			{
				++chunkIterator;
				continue;
			}

			chunkIterator = m_loadedChunks.erase(chunkIterator);
			m_removedChunks.emplace_back(chunkID);
		}
	}

	void World::processLoadingChunks()
	{
		auto chunkIterator = m_loadingChunks.begin();
		while (chunkIterator != m_loadingChunks.end())
		{
			ChunkGeneration& chunkGeneration = chunkIterator->second;
			if (!chunkGeneration.threadFinished.load())
			{
				++chunkIterator;
				continue;
			}

			ChunkID chunkID = chunkIterator->first;
			if (!isChunkWithinRenderDistance(chunkID))
			{
				chunkIterator = m_loadingChunks.erase(chunkIterator);
				continue;
			}

			m_loadedChunks[chunkID] = chunkGeneration.chunk;
			m_addedChunks.emplace_back(chunkID);

			chunkIterator = m_loadingChunks.erase(chunkIterator);
		}
	}

	void World::tryLoadRenderEligableChunks()
	{
		for (int chunkX = -(int)RENDER_DISTNACE; chunkX <= (int)RENDER_DISTNACE; chunkX++)
		{
			for (int chunkZ = -(int)RENDER_DISTNACE; chunkZ <= (int)RENDER_DISTNACE; chunkZ++)
			{
				glm::ivec2 chunkCoord = m_currentCamChunkCoord + glm::ivec2(chunkX, chunkZ);
				ChunkID chunkID = chunkCoordToChunkID(chunkCoord);

				if (isChunkLoaded(chunkID) || isChunkLoading(chunkID) || !isChunkWithinRenderDistance(chunkID))
					continue;

				launchChunkGenerationThread(chunkID);
			}
		}
	}

	bool World::isChunkWithinRenderDistance(ChunkID chunkID) const
	{
		glm::vec2 chunkMiddle = glm::vec2(chunkIDToChunkCoord(chunkID));
		glm::vec2 camChunkMiddle = glm::vec2(m_currentCamChunkCoord);
		return glm::length2(chunkMiddle - camChunkMiddle) < RENDER_DISTNACE * RENDER_DISTNACE; // wrong lmao
	}

	bool World::isChunkLoading(ChunkID chunkID) const
	{
		return m_loadingChunks.contains(chunkID);
	}

	const std::vector<ChunkID>& World::getAddedChunks() const
	{
		return m_addedChunks;
	}

	const std::vector<ChunkID>& World::getRemovedChunks() const
	{
		return m_removedChunks;
	}
	
	static void generateChunk(Chunk& chunk, ChunkID chunkID)
	{
		for (uint32_t x = 0; x < CHUNK_WIDTH; x++)
		{
			for (uint32_t z = 0; z < CHUNK_WIDTH; z++)
			{
				//glm::ivec3 blockCoord = chunkBlockCoordToBlockCoord(chunkID, { x, 0, z });

				float height1 = glm::sin((x / 30.f) * glm::pi<float>()) * 32.f;
				float height2 = glm::sin((z / 30.f) * glm::pi<float>()) * 32.f;

				height1 = glm::abs(height1);
				height2 = glm::abs(height2);

				uint32_t columnHeight = (uint32_t)glm::floor(20.f + (height1 + height2) * 0.5f);

				uint32_t grassDepth = 4;
				uint32_t stoneHeight = (uint32_t)glm::max((int)columnHeight - (int)grassDepth, 0);

				for (uint32_t y = 0; y < stoneHeight; y++)
				{
					chunk.blocks[chunkBlockCoordToChunkBlockIdx({ x, y, z })] = BlockType::STONE;
				}

				for (uint32_t y = stoneHeight; y < columnHeight; y++)
				{
					chunk.blocks[chunkBlockCoordToChunkBlockIdx({ x, y, z })] = y < columnHeight - 1 ? BlockType::DIRT : BlockType::GRASS;
				}
			}
		}
	}

	static void startChunkGeneration(World::ChunkGeneration* pChunkGeneration)
	{
		generateChunk(pChunkGeneration->chunk, pChunkGeneration->chunkID);
		pChunkGeneration->threadFinished.store(true);
	}

	void World::launchChunkGenerationThread(ChunkID chunkID)
	{
		ChunkGeneration* pChunkGeneration = &m_loadingChunks[chunkID];
		pChunkGeneration->chunkID = chunkID;
		pChunkGeneration->threadFinished.store(false);

		ThreadPool::queueJob([=]()
		{
			startChunkGeneration(pChunkGeneration);
		});
	}
}
