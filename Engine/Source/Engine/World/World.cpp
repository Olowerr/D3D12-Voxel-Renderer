#include "World.h"
#include "Engine/Application/Input.h"
#include "Engine/Application/Window.h"
#include "Camera.h"

#include <shared_mutex>

namespace Okay
{
	static const uint32_t RENDER_DISTNACE = 32;
	static std::shared_mutex mutis;
	static std::unordered_map<StructureType, StructureDescription> s_structureDescriptions;

	void World::initialize()
	{
		uint32_t numThreads = glm::max(uint32_t(std::thread::hardware_concurrency() * 0.5), 1u);
		m_threadPool.initialize(numThreads);

		m_worldGenData.terrrainNoiseInterpolation.addPoint(-0.45f, -0.55f);
		m_worldGenData.terrrainNoiseInterpolation.addPoint(-0.1f, 0.f);
		m_worldGenData.terrrainNoiseInterpolation.addPoint(0.f, 0.1f);
		m_worldGenData.terrrainNoiseInterpolation.addPoint(0.275f, 0.15f);
		m_worldGenData.terrrainNoiseInterpolation.addPoint(0.65f, 0.525f);

		m_worldGenData.terrainNoiseData.numOctaves = 4;
		m_worldGenData.terrainNoiseData.frequencyNumerator = 1.f;
		m_worldGenData.terrainNoiseData.frequencyDenominator = 150.f;

		m_worldGenData.treeNoiseData.numOctaves = 2;
		m_worldGenData.treeNoiseData.frequencyNumerator = 0.835f;
		m_worldGenData.treeNoiseData.frequencyDenominator = 1.f;
		m_worldGenData.treeNoiseData.exponent = 3.39f;

		m_worldGenData.treeAreaNoiseData.frequencyDenominator = 100.f;
		m_worldGenData.treeAreaNoiseThreshold = 0.46f;
		m_worldGenData.treeMaxSpawnAltitude = 83;

		applySeed();

		s_structureDescriptions[StructureType::TREE] = createTreeDescription();
	}

	void World::shutdown()
	{
		m_threadPool.shutdown();
	}

	void World::update(const Camera& camera)
	{
		clearUpdatedChunks();
		m_currentCamChunkCoord = chunkIDToChunkCoord(blockCoordToChunkID(camera.transform.position));

		std::unique_lock lock(mutis);
		unloadDistantChunks();
		processLoadingChunks();
		tryLoadRenderEligableChunks(camera);
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

	bool World::isBlockCoordSolid(const glm::ivec3& blockCoord) const
	{
		if (blockCoord.y < 0 || blockCoord.y >= WORLD_HEIGHT)
			return false;

		glm::ivec3 chunkBlockCoord = blockCoordToChunkBlockCoord(blockCoord);
		uint32_t chunkBlockIdx = chunkBlockCoordToChunkBlockIdx(chunkBlockCoord);

		BlockType block = tryGetBlock(blockCoordToChunkID(blockCoord), chunkBlockIdx);
		return block != BlockType::AIR && block != BlockType::WATER && block != BlockType::OAK_LEAVES;
	}

	bool World::shouldPlaceTree(const glm::ivec3& blockCoord) const
	{
		if (blockCoord.y < (int)m_worldGenData.oceanHeight || blockCoord.y > (int)m_worldGenData.treeMaxSpawnAltitude)
			return false;

		float areaNoise = Noise::samplePerlin2D_zeroOne((float)blockCoord.x, (float)blockCoord.z, m_worldGenData.treeAreaNoiseData);
		if (areaNoise < m_worldGenData.treeAreaNoiseThreshold)
			return false;

		float noise = Noise::samplePerlin2D_zeroOne((float)blockCoord.x, (float)blockCoord.z, m_worldGenData.treeNoiseData);
		return noise >= m_worldGenData.treeThreshold;
	}

	void World::loadChunkStructures(ChunkID chunkID)
	{
		glm::ivec3 chunkBlockCoord = glm::ivec3(0);
		for (chunkBlockCoord.x = 0; chunkBlockCoord.x < (int)CHUNK_WIDTH; chunkBlockCoord.x++)
		{
			for (chunkBlockCoord.z = 0; chunkBlockCoord.z < (int)CHUNK_WIDTH; chunkBlockCoord.z++)
			{
				glm::ivec3 blockCoord = chunkBlockCoordToBlockCoord(chunkID, chunkBlockCoord);
				blockCoord.y = (int)findColoumnHeight(blockCoord);

				if (!shouldPlaceTree(blockCoord))
					continue;

				ChunkStructures& chunkStructures = m_chunksStructures[chunkID];
				uint32_t nextIdx = chunkStructures.numStructures;
				if (nextIdx >= MAX_CHUNK_STRUCTURES)
					return;

				const StructureDescription& treeDesc = s_structureDescriptions[StructureType::TREE];

				// vec division in glm is defined as: vec * (1 / scalar), so the result is always 0 when using interger types and the scalar > 1 .-.
				glm::ivec3 halfXZMaxBounds = (glm::vec3)glm::ivec3(treeDesc.boundsMax.x, 0, treeDesc.boundsMax.z) / 2.f;

				chunkStructures.structures[nextIdx].type = StructureType::TREE;
				chunkStructures.structures[nextIdx].worldBoundsMin = blockCoord - halfXZMaxBounds;
				chunkStructures.structures[nextIdx].worldBoundsMax = (blockCoord + treeDesc.boundsMax) - halfXZMaxBounds;
				chunkStructures.numStructures++;
			}
		}
	}

	BlockType World::searchChunkForStructure(ChunkID chunkID, const glm::ivec3& blockCoord) const
	{
		const auto& chunkIt = m_chunksStructures.find(chunkID);
		if (chunkIt == m_chunksStructures.end())
			return BlockType::INVALID;

		const ChunkStructures& chunkStructures = chunkIt->second;
		for (const Structure& structure : chunkStructures.structures)
		{
			if (!structure.isWithinBounds(blockCoord))
				continue;

			glm::ivec3 localBlockCoord = blockCoord - structure.worldBoundsMin;
			for (const BlockDescription& blockDesc : s_structureDescriptions[structure.type].blocks)
			{
				if (blockDesc.position == localBlockCoord)
					return blockDesc.type;
			}
		}

		return BlockType::INVALID;
	}

	BlockType World::tryFindStructureBlock(const glm::ivec3& blockCoord) const
	{
		std::shared_lock lock(mutis);

		const int searchWidth = 1;
		ChunkID chunkID = blockCoordToChunkID(blockCoord);
		glm::ivec2 chunkCoord = chunkIDToChunkCoord(chunkID);

		glm::ivec2 offset = {};
		for (offset.x = -searchWidth; offset.x <= searchWidth; offset.x++)
		{
			for (offset.y = -searchWidth; offset.y <= searchWidth; offset.y++)
			{
				ChunkID adjacentChunkID = chunkCoordToChunkID(chunkCoord + offset);
				BlockType block = searchChunkForStructure(adjacentChunkID, blockCoord);

				if (block != BlockType::INVALID)
					return block;
			}
		}

		return BlockType::INVALID;
	}

	uint32_t World::findColoumnHeight(const glm::ivec3& blockCoordXZ)
	{
		float noise = Noise::samplePerlin2D_minusOneOne((float)blockCoordXZ.x, (float)blockCoordXZ.z, m_worldGenData.terrainNoiseData);
		noise = m_worldGenData.terrrainNoiseInterpolation.sample(noise);

		float scaledNoise = noise * m_worldGenData.amplitude + m_worldGenData.oceanHeight;
		uint32_t columnHeight = (uint32_t)glm::clamp((int)scaledNoise, 1, (int)WORLD_HEIGHT);

		return columnHeight;
	}

	BlockType World::generateBlock(const glm::ivec3& blockCoord)
	{
		int columnHeight = (int)findColoumnHeight(blockCoord);
		int grassDepth = 4;
		int stoneHeight = glm::max((int)columnHeight - (int)grassDepth, 0);

		if (blockCoord.y < stoneHeight)
			return BlockType::STONE;

		if (blockCoord.y >= stoneHeight && blockCoord.y < columnHeight)
		{
			bool belowGround = blockCoord.y < columnHeight - 1;
			bool structureAbove = tryFindStructureBlock(blockCoord + glm::ivec3(0, 1, 0)) != BlockType::INVALID;
			return belowGround || structureAbove ? BlockType::DIRT : BlockType::GRASS;
		}
		
		if (blockCoord.y >= columnHeight && blockCoord.y < (int)m_worldGenData.oceanHeight)
			return BlockType::WATER;
	
		BlockType structureBlock = tryFindStructureBlock(blockCoord);
		if (structureBlock != BlockType::INVALID)
			return structureBlock;

		return BlockType::AIR;
	}

	void World::applySeed() const
	{
		Noise::applyPerlinSeed(m_worldGenData.seed);
	}

	void World::resetWorld()
	{
		std::unique_lock lock(mutis);

		m_loadedChunks.clear();
		m_chunksStructures.clear();
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
			m_chunksStructures.erase(chunkID);

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

	void World::tryLoadRenderEligableChunks(const Camera& camera)
	{
		for (int i = 0; i < RENDER_DISTNACE; i++)
		{
			uint32_t loadedChunks = 0;
			uint32_t totalChunks = 0;

			for (int chunkX = -i; chunkX <= i; chunkX++)
			{
				int zIncrement = chunkX == -i || chunkX == i ? 1 : i * 2;

				for (int chunkZ = -i; chunkZ <= i; chunkZ += zIncrement)
				{
					glm::ivec2 chunkCoord = m_currentCamChunkCoord + glm::ivec2(chunkX, chunkZ);
					ChunkID chunkID = chunkCoordToChunkID(chunkCoord);
					
					if (!isChunkWithinRenderDistance(chunkID) || !isChunkInView(camera, chunkID))
						continue;

					totalChunks++;

					if (isChunkLoaded(chunkID))
					{
						loadedChunks++;
						continue;
					}

					if (isChunkLoading(chunkID))
						continue;
					
					launchChunkGenerationThread(chunkID);
				}
			}

			if (loadedChunks != totalChunks)
				break;
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

	bool World::isChunkInView(const Camera& camera, ChunkID chunkID) const
	{
		glm::vec3 chunkExtents = glm::vec3(CHUNK_WIDTH, WORLD_HEIGHT, CHUNK_WIDTH) * 0.5f;
		glm::vec3 chunkCenter = chunkCoordToWorldCoord(chunkIDToChunkCoord(chunkID));
		chunkCenter += chunkExtents;

		Collision::AABB chunkBox = Collision::createAABB(chunkCenter, chunkExtents);
		return Collision::frustumAABB(camera.frustum, chunkBox);
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

		for (uint32_t i = 0; i < MAX_BLOCKS_IN_CHUNK; i++)
		{
			glm::ivec3 chunkBlockCoord = chunkBlockIdxToChunkBlockCoord(i);
			glm::ivec3 blockCoord = chunkBlockCoordToBlockCoord(chunkID, chunkBlockCoord);
			chunk.blocks[i] = generateBlock(blockCoord);
		}
	}

	void World::launchChunkGenerationThread(ChunkID chunkID)
	{
		const int loadWidth = 1;
		glm::ivec2 chunkCoord = chunkIDToChunkCoord(chunkID);
		for (int offsetX = -loadWidth; offsetX <= loadWidth; offsetX++)
		{
			for (int offsetZ = -loadWidth; offsetZ <= loadWidth; offsetZ++)
			{
				ChunkID adjacentChunkID = chunkCoordToChunkID(chunkCoord + glm::ivec2(offsetX, offsetZ));
				loadChunkStructures(adjacentChunkID);
			}
		}

		ChunkGeneration& chunkGeneration = m_loadingChunks[chunkID];
		chunkGeneration.chunkID = chunkID;
		chunkGeneration.threadFinished.store(false);

		ChunkGeneration* pChunkGeneration = &chunkGeneration;
		m_threadPool.queueJob([=]()
		{
			generateChunk(pChunkGeneration);
			pChunkGeneration->threadFinished.store(true);
		});
	}
}
