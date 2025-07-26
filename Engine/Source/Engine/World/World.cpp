#include "World.h"
#include "../Application/Time.h"
#include "../Application/Input.h"

#include <shared_mutex>
#include <chrono>

namespace Okay
{
	static const int RENDER_DISTNACE = 8;
	static const int MAX_CHUNKS_PROCCESSED_PER_FRAME = 1;
	static std::shared_mutex mutis;

	World::World()
	{
		m_loadingChunks.reserve(std::thread::hardware_concurrency() / 2);
	}

	World::~World()
	{
	}

	void World::update()
	{
		clearUpdatedChunks();
		std::unique_lock lock(mutis);
		
		m_currentCamChunkCoord = chunkIDToChunkCoord(blockCoordToChunkID(m_camera.transform.position));

		auto removeChunkIterator = m_loadedChunks.begin();
		while (removeChunkIterator != m_loadedChunks.end())
		{
			ChunkID chunkID = removeChunkIterator->first;
			glm::ivec2 chunkCoord = chunkIDToChunkCoord(chunkID);

			if (isChunkWithinRenderDistance(chunkCoord))
			{
				++removeChunkIterator;
				continue;
			}

			removeChunkIterator = m_loadedChunks.erase(removeChunkIterator);
			m_removedChunks.emplace_back(chunkID);
		}


		auto loadingChunkIterator = m_loadingChunks.begin();
		while (loadingChunkIterator != m_loadingChunks.end())
		{
			ChunkGeneration& chunkGeneration = loadingChunkIterator->second;
			if (!chunkGeneration.threadFinished.load())
			{
				++loadingChunkIterator;
				continue;
			}

			chunkGeneration.genThread.join();

			ChunkID chunkID = loadingChunkIterator->first;
			if (!isChunkWithinRenderDistance(chunkIDToChunkCoord(chunkID)))
			{
				loadingChunkIterator = m_loadingChunks.erase(loadingChunkIterator);
				continue;
			}

			m_loadedChunks[chunkID] = chunkGeneration.chunk;
			m_addedChunks.emplace_back(chunkID);

			loadingChunkIterator = m_loadingChunks.erase(loadingChunkIterator);
		}

		uint32_t chunksLaunched = 0;
		for (int chunkX = -RENDER_DISTNACE; chunkX <= RENDER_DISTNACE; chunkX++)
		{
			for (int chunkZ = -RENDER_DISTNACE; chunkZ <= RENDER_DISTNACE; chunkZ++)
			{
				glm::ivec2 chunkCoord = m_currentCamChunkCoord + glm::ivec2(chunkX, chunkZ);
				ChunkID chunkID = chunkCoordToChunkID(chunkCoord);

				if (isChunkLoaded(chunkID) || isChunkLoading(chunkID) || !isChunkWithinRenderDistance(chunkCoord))
					continue;

				if (m_loadingChunks.size() == m_loadingChunks.max_size())
					continue;


				launchChunkGenerationThread(chunkID);

				if (++chunksLaunched == MAX_CHUNKS_PROCCESSED_PER_FRAME)
					return;
			}
		}
	}

	uint8_t World::getBlockAtBlockCoord(const glm::ivec3& blockCoord) const
	{
		if (blockCoord.y < 0 || blockCoord.y >= WORLD_HEIGHT)
			return 0;

		ChunkID chunkID = blockCoordToChunkID(blockCoord);

		glm::ivec3 chunkBlockCoord = blockCoordToChunkBlockCoord(blockCoord);
		uint32_t chunkBlockIdx = chunkBlockCoordToChunkBlockIdx(chunkBlockCoord);

		return tryGetBlock(chunkID, chunkBlockIdx);
	}

	uint8_t World::tryGetBlock(ChunkID chunkID, uint32_t blockIdx) const
	{
		std::shared_lock lock(mutis);
		const Chunk* pChunk = tryGetChunk(chunkID);
		return pChunk ? pChunk->blocks[blockIdx] : INVALID_UINT8;
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

	bool World::isChunkWithinRenderDistance(const glm::ivec2& chunkCoord) const
	{
		glm::vec2 chunkMiddle = glm::vec2(chunkCoord);
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

				float height1 = glm::cos((x / 15.f) * glm::pi<float>()) * 16.f;
				float height2 = glm::cos((z / 15.f) * glm::pi<float>()) * 16.f;

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

	static void startChunkGeneration(World::ChunkGeneration* pChunkGeneration)
	{
		generateChunk(pChunkGeneration->chunk, pChunkGeneration->chunkID);
		pChunkGeneration->threadFinished.store(true);
	}

	void World::launchChunkGenerationThread(ChunkID chunkID)
	{
		ChunkGeneration& chunkGeneration = m_loadingChunks[chunkID];
		chunkGeneration.chunkID = chunkID;
		chunkGeneration.threadFinished.store(false);

		chunkGeneration.genThread = std::thread(startChunkGeneration, &chunkGeneration);
	}
}
