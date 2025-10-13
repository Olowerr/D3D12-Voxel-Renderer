#include "World.h"
#include "Engine/Application/Input.h"
#include "Engine/Application/Window.h"
#include "Camera.h"
#include "Engine/Utilities/Random.h"

#include <shared_mutex>

namespace Okay
{
	const float CloudGenerationData::UPDATE_INTERVAL = 10.f;

	static std::shared_mutex mutis;
	static std::unordered_map<StructureType, StructureDescription> s_structureDescriptions;

	void World::initialize()
	{
		applySeed();

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

		s_structureDescriptions[StructureType::TREE] = createTreeDescription();


		m_cloudGenData.cloudNoise.numOctaves = 1;
		m_cloudGenData.cloudNoise.frequencyNumerator = 1.f;
		m_cloudGenData.cloudNoise.frequencyDenominator = 67.f;
		m_cloudGenData.cloudNoise.persistence = 0.5f;
		m_cloudGenData.cloudNoise.cutOff = 0.f;
		m_cloudGenData.cloudNoise.exponent = 1.35f;

		m_cloudGenData.maskNoise.numOctaves = 5;
		m_cloudGenData.maskNoise.frequencyNumerator = 1.f;
		m_cloudGenData.maskNoise.frequencyDenominator = 147.f;
		m_cloudGenData.maskNoise.persistence = 0.48f;
		m_cloudGenData.maskNoise.cutOff = 0.64f;
		m_cloudGenData.maskNoise.exponent = 1.f;
	}

	void World::shutdown()
	{
		m_threadPool.shutdown();
	}

	void World::update(const Camera& camera, TimeStep dt)
	{
		clearUpdatedChunks();
		m_currentCamChunkCoord = chunkIDToChunkCoord(blockCoordToChunkID(glm::floor(camera.transform.position)));

		updateClouds(camera, dt);

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

	bool World::isBlockTypeSolid(BlockType block)
	{
		// TODO: Improve this lamo
		return block != BlockType::AIR && block != BlockType::WATER && block != BlockType::OAK_LEAVES;
	}

	bool World::isBlockCoordSolid(const glm::ivec3& blockCoord) const
	{
		if (blockCoord.y < 0 || blockCoord.y >= WORLD_HEIGHT)
			return false;

		glm::ivec3 chunkBlockCoord = blockCoordToChunkBlockCoord(blockCoord);
		uint32_t chunkBlockIdx = chunkBlockCoordToChunkBlockIdx(chunkBlockCoord);

		BlockType block = tryGetBlock(blockCoordToChunkID(blockCoord), chunkBlockIdx);
		return isBlockTypeSolid(block);
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
				glm::ivec3 minBounds = blockCoord - halfXZMaxBounds;
				glm::ivec3 maxBounds = (blockCoord + treeDesc.boundsMax) - halfXZMaxBounds;

				bool loaded = false;
				for (const Structure& structure : chunkStructures.structures)
				{
					if (structure == Structure(StructureType::TREE, minBounds, maxBounds))
					{
						loaded = true;
						break;
					}
				}

				if (loaded)
					continue;

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

		return BlockType::AIR;
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
			BlockType structBlockAbove = tryFindStructureBlock(blockCoord + glm::ivec3(0, 1, 0));
			return isBlockTypeSolid(structBlockAbove) || belowGround ? BlockType::DIRT : BlockType::GRASS;
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

	void World::recreateClouds()
	{
		m_cloudGenData.cloudList.clear();
		m_cloudGenData.localDrift = glm::vec3(FLT_MAX);
		m_cloudGenData.updateTimer = CloudGenerationData::UPDATE_INTERVAL;
	}

	void World::updateClouds(const Camera& camera, TimeStep dt)
	{
		m_cloudGenData.globalDrift += m_cloudGenData.velocity * dt;
		m_cloudGenData.localDrift += m_cloudGenData.velocity * dt;

		m_cloudGenData.updateTimer += dt;
		if (m_cloudGenData.updateTimer < CloudGenerationData::UPDATE_INTERVAL)
			return;

		m_cloudGenData.updateTimer -= CloudGenerationData::UPDATE_INTERVAL;

		clearDistanceClouds(camera);
		generateCloudList(camera);

		m_cloudGenData.localDrift = glm::vec2(camera.transform.position.x, camera.transform.position.z);
	}

	void World::generateCloudList(const Camera& camera)
	{
		float viewDistance = (float)m_cloudGenData.chunkVisiblityDistance * CHUNK_WIDTH;
		glm::vec2 cameraXZPos = glm::vec2(camera.transform.position.x, camera.transform.position.z);

		glm::vec2 min = glm::min(m_cloudGenData.localDrift, cameraXZPos);
		glm::vec2 max = glm::max(m_cloudGenData.localDrift, cameraXZPos);

		glm::vec2 overlappingMin = max - glm::vec2(viewDistance);
		glm::vec2 overlappingMax = min + glm::vec2(viewDistance);

		for (float x = -viewDistance + cameraXZPos.x; x <= viewDistance + cameraXZPos.x; x += m_cloudGenData.sampleDistance)
		{
			for (float z = -viewDistance + cameraXZPos.y; z <= viewDistance + cameraXZPos.y; z += m_cloudGenData.sampleDistance)
			{
				if (x >= overlappingMin.x && x <= overlappingMax.x && z >= overlappingMin.y && z <= overlappingMax.y)
					continue;

				sampleCloud(x - m_cloudGenData.globalDrift.x, z - m_cloudGenData.globalDrift.y);
			}
		}
	}

	void World::clearDistanceClouds(const Camera& camera)
	{
		float viewDistance = (float)m_cloudGenData.chunkVisiblityDistance * CHUNK_WIDTH;
		glm::vec3 globalDriftVec3 = glm::vec3(m_cloudGenData.globalDrift.x, 0, m_cloudGenData.globalDrift.y);

		for (int32_t i = (int32_t)m_cloudGenData.cloudList.size() - 1; i >= 0; i--)
		{
			glm::vec3 cloudGlobalPos = m_cloudGenData.cloudList[i] + globalDriftVec3;
			glm::vec3 camToCloud = glm::abs(cloudGlobalPos - camera.transform.position);

			if (camToCloud.x > viewDistance || camToCloud.z > viewDistance)
			{
				m_cloudGenData.cloudList.erase(m_cloudGenData.cloudList.begin() + i);
			}
		}
	}

	void World::sampleCloud(float x, float z)
	{
		float cloudNoise = Noise::samplePerlin2D_zeroOne(x, z, m_cloudGenData.cloudNoise);
		float maskNoise = Noise::samplePerlin2D_zeroOne(x, z, m_cloudGenData.maskNoise);
		float finalNoise = cloudNoise * maskNoise;

		float cloudHeight = finalNoise * m_cloudGenData.height;

		float currentHeight = 0.f;
		while (currentHeight < cloudHeight)
		{
			uint32_t seed = uint32_t(finalNoise * UINT_MAX + currentHeight);

			glm::vec3 placementOffset = glm::vec3(
				Random::randomFloat(seed) * 2.f - 1.f,
				(Random::randomFloat(seed) * 2.f - 1.f) * 0.5f,
				Random::randomFloat(seed) * 2.f - 1.f);

			placementOffset = glm::normalize(placementOffset) * m_cloudGenData.maxOffset * Random::randomFloat(seed);
			glm::vec3 cloudPoint = glm::vec3(x, m_cloudGenData.spawnHeight + currentHeight, z);

			m_cloudGenData.cloudList.emplace_back(cloudPoint + placementOffset);
			currentHeight += m_cloudGenData.sampleDistance;
		}
	}

	const std::vector<glm::vec3>& World::getCloudList() const
	{
		return m_cloudGenData.cloudList;
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
		for (int i = 0; i < (int)m_renderDistance; i++)
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
		return glm::length2(chunkMiddle - camChunkMiddle) <= m_renderDistance * m_renderDistance;
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

				/*
					Structure loading like this is problematic cuz
					the same structure is stored multiple times due to how loadChunkStructures is called
						(function called multiple times for the same chunk)
				*/
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
