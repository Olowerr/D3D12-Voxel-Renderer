#include "World.h"
#include "../Application/Time.h"
#include "../Application/Input.h"
#include "Engine/Utilities/ThreadPool.h"

#include "sivPerlinNoise/PerlinNoise.hpp"

#include <shared_mutex>

namespace Okay
{
	static const uint32_t RENDER_DISTNACE = 32;
	static std::shared_mutex mutis;

	World::World()
	{
		m_camera.farZ = (RENDER_DISTNACE + 2) * CHUNK_WIDTH;
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

	void World::setWorldGenFrequency(float frequency)
	{
		m_worldGenFrequency = frequency;
	}

	void World::setWorldGenPersistance(float persistance)
	{
		m_worldGenPersistance = persistance;
	}

	void World::setWorldGenAmplitude(float amplitude)
	{
		m_worldGenAmplitude = amplitude;
	}

	void World::setWorldGenOctaves(uint32_t octaves)
	{
		m_worldGenOctaves = octaves;
	}

	void World::setWorldGenSeed(uint32_t seed)
	{
		m_worldGenSeed = seed;
	}

	void World::reloadWorld()
	{
		std::unique_lock lock(mutis);

		m_loadedChunks.clear();

		for (auto& loadingChunkData : m_loadingChunks)
		{
			ChunkGeneration& chunkGeneration = loadingChunkData.second;
			chunkGeneration.cancel.store(true);
		}
	}

	float World::getWorldGenFrequency() const
	{
		return m_worldGenFrequency;
	}

	float World::getWorldGenPersistance() const
	{
		return m_worldGenPersistance;
	}

	float World::getWorldGenAmplitude() const
	{
		return m_worldGenAmplitude;
	}

	uint32_t World::getWorldGenOctaves() const
	{
		return m_worldGenOctaves;
	}

	uint32_t World::getWorldGenSeed() const
	{
		return m_worldGenSeed;
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
			if (!isChunkWithinRenderDistance(chunkID) || chunkGeneration.cancel.load())
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
		for (int i = 0; i < RENDER_DISTNACE; i++)
		{
			for (int chunkX = -i; chunkX <= i; chunkX++)
			{
				int zIncrement = chunkX == -i || chunkX == i ? 1 : i * 2;

				for (int chunkZ = -i; chunkZ <= i; chunkZ += zIncrement)
				{
					glm::ivec2 chunkCoord = m_currentCamChunkCoord + glm::ivec2(chunkX, chunkZ);
					ChunkID chunkID = chunkCoordToChunkID(chunkCoord);

					if (isChunkLoaded(chunkID) || isChunkLoading(chunkID) || !isChunkWithinRenderDistance(chunkID))
						continue;

					launchChunkGenerationThread(chunkID);
				}
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
	
	void World::generateChunk(ChunkGeneration* pChunkGeneration)
	{
		ChunkID chunkID = pChunkGeneration->chunkID;
		Chunk& chunk = pChunkGeneration->chunk;
		std::atomic<bool>& cancel = pChunkGeneration->cancel;

		siv::PerlinNoise::seed_type seed = m_worldGenSeed;
		siv::PerlinNoise perlin{ seed };

		for (uint32_t x = 0; x < CHUNK_WIDTH; x++)
		{
			for (uint32_t z = 0; z < CHUNK_WIDTH; z++)
			{
				if (cancel.load())
					return;

				glm::ivec3 blockCoord = chunkBlockCoordToBlockCoord(chunkID, { x, 0, z });
				float noise = perlin.octave2D_01(blockCoord.x * m_worldGenFrequency, blockCoord.z * m_worldGenFrequency, m_worldGenOctaves);
				noise = glm::pow(noise, 5.f);

				uint32_t columnHeight = glm::min(uint32_t(noise * m_worldGenAmplitude + 1), WORLD_HEIGHT);

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

	void World::launchChunkGenerationThread(ChunkID chunkID)
	{
		ChunkGeneration& chunkGeneration = m_loadingChunks[chunkID];
		chunkGeneration.chunkID = chunkID;
		chunkGeneration.threadFinished.store(false);
		chunkGeneration.cancel.store(false);

		ChunkGeneration* pChunkGeneration = &chunkGeneration;
		ThreadPool::queueJob([=]()
		{
			generateChunk(pChunkGeneration);
			pChunkGeneration->threadFinished.store(true);
		});
	}
}
