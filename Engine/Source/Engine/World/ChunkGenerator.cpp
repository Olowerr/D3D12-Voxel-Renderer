#include "ChunkGenerator.h"
#include "World.h"
#include "Camera.h"
#include "Engine/Utilities/Noise.h"
#include "Engine/World/WorldGenSettings.h"
#include "Engine/Utilities/Utilities.h"
#include "Engine/Utilities/Random.h"

namespace Okay
{
	static std::unordered_map<StructureType, StructureDescription> s_structureDescriptions;
	static ChunkGenID s_chunkGenID = 0;

	void ChunkGenerator::initialize(uint64_t seed, const BlockTextureIDs& blockTextureIDs, const World& world)
	{
		m_threadPool.initialize(std::thread::hardware_concurrency());
		s_structureDescriptions[StructureType::TREE] = createTreeDescription();
		m_pBlockTextureIds = &blockTextureIDs;
		m_pWorld = &world;
	}

	void ChunkGenerator::shutdown()
	{
		m_threadPool.shutdown();
	}

	void ChunkGenerator::applySeed(uint64_t seed)
	{
		// Temp
		Noise::applyPerlinSeed(seed);
	}

	void ChunkGenerator::update(const Camera& camera)
	{
		if (WorldGenerationData::get().pauseGen)
			return;

		processLoadingChunks();
		tryLoadRenderEligableChunks(camera);
	}

	void ChunkGenerator::processLoadingChunks()
	{
		for (ChunkGenID chunkGenID : m_completedIDs)
		{
			ChunkGenerationThread& chunkGenThread = m_loadingChunks[chunkGenID];
			ChunkID chunkID = chunkGenThread.chunkID;

			if (chunkGenThread.blocksGenerated)
				handleMeshUpdates(chunkID);

			m_loadingChunks.erase(chunkGenID);
			tryRemoveLastMeshGenID(chunkID);
		}
		m_completedIDs.clear();


		auto chunkIt = m_loadingChunks.begin();
		while (chunkIt != m_loadingChunks.end())
		{
			ChunkGenerationThread& chunkGen = chunkIt->second;
			if (!chunkGen.threadFinished)
			{
				chunkIt++;
				continue;
			}

			ChunkID chunkID = chunkGen.chunkID;
			if (chunkGen.meshGenID != m_lastMeshGenIDs[chunkID])
			{
				chunkIt = m_loadingChunks.erase(chunkIt);
				tryRemoveLastMeshGenID(chunkID);
				continue;
			}

			m_completedIDs.emplace_back(chunkIt->first);
			chunkIt++;
		}


		for (auto& chunkIt : m_chunkMeshUpdates)
			queueMeshUpdate(chunkIt.first);
		m_chunkMeshUpdates.clear();
	}

	void ChunkGenerator::tryLoadRenderEligableChunks(const Camera& camera)
	{
		glm::ivec2 camChunkCoord = vec3CoordToChunkCoord(camera.transform.position);
		int renderDistance = (int)WorldGenerationData::get().renderDistance;

		for (int i = 0; i < renderDistance; i++)
		{
			uint32_t loadedChunks = 0;
			uint32_t totalChunks = 0;

			for (int chunkX = -i; chunkX <= i; chunkX++)
			{
				int zIncrement = chunkX == -i || chunkX == i ? 1 : i * 2;

				for (int chunkZ = -i; chunkZ <= i; chunkZ += zIncrement)
				{
					glm::ivec2 chunkCoord = camChunkCoord + glm::ivec2(chunkX, chunkZ);
					ChunkID chunkID = chunkCoordToChunkID(chunkCoord);

					if (!isChunkWithinRenderDistance(chunkID, camChunkCoord) || !isChunkInView(chunkID, camera))
						continue;

					totalChunks++;

					if (m_pWorld->isChunkLoaded(chunkID))
					{
						loadedChunks++;
						continue;
					}

					if (m_lastMeshGenIDs.contains(chunkID))
						continue;

					queueChunkGeneration(chunkID);
				}
			}

			if (loadedChunks != totalChunks)
				break;
		}
	}

	void ChunkGenerator::queueChunkGeneration(ChunkID chunkID)
	{
		ChunkGenerationThread& chunkGeneration = m_loadingChunks[s_chunkGenID++];
		chunkGeneration.chunkID = chunkID;
		chunkGeneration.meshGenID = m_lastMeshGenIDs[chunkID] = 0;
		chunkGeneration.threadFinished = false;
		chunkGeneration.blocksGenerated = false;

		ChunkGenerationThread* pChunkGen = &chunkGeneration;
		m_threadPool.queueJob([=]()
			{
				generateBlockData(*pChunkGen);
				pChunkGen->blocksGenerated = true;
				pChunkGen->threadFinished = true;
			});
	}

	const std::vector<ChunkGenID>& ChunkGenerator::getCompletedChunks() const
	{
		return m_completedIDs;
	}

	const ChunkGenerationThread& ChunkGenerator::getChunkGenData(ChunkGenID chunkGenID) const
	{
		return m_loadingChunks.find(chunkGenID)->second;
	}

	bool ChunkGenerator::areChunksLoading() const
	{
		return m_loadingChunks.size() > 0;
	}

	uint32_t ChunkGenerator::findColoumnHeight(const glm::ivec3& blockCoordXZ) const
	{
		WorldGenerationData& worldGenData = WorldGenerationData::get();

		float noise = Noise::samplePerlin2D_minusOneOne((float)blockCoordXZ.x, (float)blockCoordXZ.z, worldGenData.terrainNoiseData);
		noise = worldGenData.terrrainNoiseInterpolation.sample(noise);

		float scaledNoise = noise * worldGenData.amplitude + worldGenData.oceanHeight;
		uint32_t columnHeight = (uint32_t)glm::clamp((int)scaledNoise, 1, (int)WORLD_HEIGHT);

		return columnHeight;
	}

	bool ChunkGenerator::shouldPlaceTree(const glm::ivec3& blockCoord) const
	{
		WorldGenerationData& worldGenData = WorldGenerationData::get();

		if (blockCoord.y < (int)worldGenData.oceanHeight || blockCoord.y >(int)worldGenData.treeMaxSpawnAltitude)
			return false;

		float areaNoise = Noise::samplePerlin2D_zeroOne((float)blockCoord.x, (float)blockCoord.z, worldGenData.treeAreaNoiseData);
		if (areaNoise < worldGenData.treeAreaNoiseThreshold)
			return false;

		float noise = Noise::samplePerlin2D_zeroOne((float)blockCoord.x, (float)blockCoord.z, worldGenData.treeNoiseData);
		return noise >= worldGenData.treeThreshold;
	}

	bool ChunkGenerator::isBlockTypeSolid(BlockType block)
	{
		// TODO: Improve this lamo
		return block != BlockType::INVALID && block != BlockType::AIR && block != BlockType::WATER && block != BlockType::OAK_LEAVES;
	}

	BlockType ChunkGenerator::searchChunkForStructure(ChunkGenerationThread& chunkGeneration, const glm::ivec3& blockCoord) const
	{
		for (const Structure& structure : chunkGeneration.structures)
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

	BlockType ChunkGenerator::tryFindStructureBlock(ChunkGenerationThread& chunkGeneration, const glm::ivec3& blockCoord) const
	{
		ChunkID chunkID = blockCoordToChunkID(blockCoord);
		BlockType block = searchChunkForStructure(chunkGeneration, blockCoord);
		return block;
	}

	BlockType ChunkGenerator::generateBlock(ChunkGenerationThread& chunkGeneration, const glm::ivec3& blockCoord) const
	{
		int columnHeight = (int)findColoumnHeight(blockCoord);
		int grassDepth = 4;
		int stoneHeight = glm::max((int)columnHeight - (int)grassDepth, 0);

		if (blockCoord.y < stoneHeight)
			return BlockType::STONE;

		if (blockCoord.y >= stoneHeight && blockCoord.y < columnHeight)
		{
			bool belowGround = blockCoord.y < columnHeight - 1;
			BlockType structBlockAbove = tryFindStructureBlock(chunkGeneration, blockCoord + glm::ivec3(0, 1, 0));
			return isBlockTypeSolid(structBlockAbove) || belowGround ? BlockType::DIRT : BlockType::GRASS;
		}

		WorldGenerationData& worldGenData = WorldGenerationData::get();
		if (blockCoord.y >= columnHeight && blockCoord.y < (int)worldGenData.oceanHeight)
			return BlockType::WATER;

		BlockType structureBlock = tryFindStructureBlock(chunkGeneration, blockCoord);
		if (structureBlock != BlockType::INVALID)
			return structureBlock;

		return BlockType::AIR;
	}

	void ChunkGenerator::cacheStructures(ChunkGenerationThread& chunkGeneration, ChunkID sourceChunkID)
	{
		glm::ivec3 chunkBlockCoord = glm::ivec3(0);
		for (chunkBlockCoord.x = 0; chunkBlockCoord.x < (int)CHUNK_WIDTH; chunkBlockCoord.x++)
		{
			for (chunkBlockCoord.z = 0; chunkBlockCoord.z < (int)CHUNK_WIDTH; chunkBlockCoord.z++)
			{
				glm::ivec3 blockCoord = chunkBlockCoordToBlockCoord(sourceChunkID, chunkBlockCoord);
				blockCoord.y = (int)findColoumnHeight(blockCoord);

				if (!shouldPlaceTree(blockCoord))
					continue;

				const StructureDescription& treeDesc = s_structureDescriptions[StructureType::TREE];

				// vec division in glm is defined as: vec * (1 / scalar), so the result is always 0 when using interger types and the scalar > 1 .-.
				glm::ivec3 halfXZMaxBounds = (glm::vec3)glm::ivec3(treeDesc.boundsMax.x, 0, treeDesc.boundsMax.z) / 2.f;
				glm::ivec3 minBounds = blockCoord - halfXZMaxBounds;
				glm::ivec3 maxBounds = (blockCoord + treeDesc.boundsMax) - halfXZMaxBounds;

				bool loaded = false;
				for (const Structure& structure : chunkGeneration.structures)
				{
					if (structure == Structure(StructureType::TREE, minBounds, maxBounds))
					{
						loaded = true;
						break;
					}
				}

				if (loaded)
					continue;

				Structure& structure = chunkGeneration.structures.emplace_back();
				structure.type = StructureType::TREE;
				structure.worldBoundsMin = blockCoord - halfXZMaxBounds;
				structure.worldBoundsMax = (blockCoord + treeDesc.boundsMax) - halfXZMaxBounds;
			}
		}
	}

	static void addVertex(MeshData& meshData, Vertex newVertex)
	{
		uint32_t idx = INVALID_UINT32;
		int startIdx = glm::max((int)meshData.vertices.size() - 5, 0); // Lmao
		for (uint64_t i = startIdx; i < meshData.vertices.size(); i++)
		{
			if (meshData.vertices[i] == newVertex)
			{
				idx = (uint32_t)i;
				break;
			}
		}

		if (idx == INVALID_UINT32)
		{
			meshData.indices.emplace_back((uint32_t)meshData.vertices.size());
			meshData.vertices.emplace_back(newVertex);
		}
		else
		{
			meshData.indices.emplace_back(idx);
		}
	}

	bool ChunkGenerator::checkSolidBlock(ChunkGenerationThread& chunkGeneration, const glm::ivec3& blockCoord)
	{
		if (blockCoord.y < 0 || blockCoord.y >= WORLD_HEIGHT)
			return false;

		BlockType block = BlockType::INVALID;

		ChunkID readChunkID = blockCoordToChunkID(blockCoord);
		glm::ivec3 chunkBlockCoord = blockCoordToChunkBlockCoord(blockCoord);
		uint32_t chunkBlockIdx = chunkBlockCoordToChunkBlockIdx(chunkBlockCoord);

		if (readChunkID == chunkGeneration.chunkID)
		{
			block = chunkGeneration.chunkData.blocks[chunkBlockIdx];
		}
		else
		{
			for (const ChunkGenerationThread::ChunkRef& chunkRef : chunkGeneration.chunkRefs)
			{
				if (chunkRef.chunkID == readChunkID)
				{
					block = chunkRef.pChunk->blocks[chunkBlockIdx];
					break;
				}
			}

			if (block == BlockType::INVALID)
				return true;
		}

		return ChunkGenerator::isBlockTypeSolid(block);
	}

	uint32_t ChunkGenerator::getTextureID(BlockType blockType, BlockSide blockSide) const
	{
		return m_pBlockTextureIds->find(blockType)->second.IDs[blockSide];
	}

	void ChunkGenerator::addBlockMeshData(ChunkGenerationThread& chunkGeneration, BlockType block, const glm::ivec3& worldBlockCoord, MeshData& outMesh)
	{
		// Top
		if (!checkSolidBlock(chunkGeneration, worldBlockCoord + UP_DIR))
		{
			uint32_t textureId = getTextureID(block, BlockSide::TOP);
			glm::ivec3 chunkBlockCoord = blockCoordToChunkBlockCoord(worldBlockCoord);

			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 0), glm::vec2(1, 0), textureId, 0));
			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 1), glm::vec2(1, 1), textureId, 0));
			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 1), glm::vec2(0, 1), textureId, 0));

			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 1), glm::vec2(0, 1), textureId, 0));
			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 0), glm::vec2(0, 0), textureId, 0));
			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 0), glm::vec2(1, 0), textureId, 0));
		}

		// Bottom
		if (!checkSolidBlock(chunkGeneration, worldBlockCoord - UP_DIR))
		{
			uint32_t textureId = getTextureID(block, BlockSide::BOTTOM);
			glm::ivec3 chunkBlockCoord = blockCoordToChunkBlockCoord(worldBlockCoord);

			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(1, 0, 1), glm::vec2(1, 1), textureId, 1));
			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 1), glm::vec2(1, 0), textureId, 1));
			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 0), glm::vec2(0, 0), textureId, 1));

			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 0), glm::vec2(0, 0), textureId, 1));
			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(1, 0, 0), glm::vec2(0, 1), textureId, 1));
			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(1, 0, 1), glm::vec2(1, 1), textureId, 1));
		}

		// Right
		if (!checkSolidBlock(chunkGeneration, worldBlockCoord + RIGHT_DIR))
		{
			uint32_t textureId = getTextureID(block, BlockSide::SIDE);
			glm::ivec3 chunkBlockCoord = blockCoordToChunkBlockCoord(worldBlockCoord);

			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 0), glm::vec2(0, 0), textureId, 2));
			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 1), glm::vec2(1, 0), textureId, 2));
			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(1, 0, 1), glm::vec2(1, 1), textureId, 2));

			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 0), glm::vec2(0, 0), textureId, 2));
			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(1, 0, 1), glm::vec2(1, 1), textureId, 2));
			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(1, 0, 0), glm::vec2(0, 1), textureId, 2));
		}

		// Left
		if (!checkSolidBlock(chunkGeneration, worldBlockCoord - RIGHT_DIR))
		{
			uint32_t textureId = getTextureID(block, BlockSide::SIDE);
			glm::ivec3 chunkBlockCoord = blockCoordToChunkBlockCoord(worldBlockCoord);

			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 1), glm::vec2(0, 1), textureId, 3));
			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 1), glm::vec2(0, 0), textureId, 3));
			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 0), glm::vec2(1, 0), textureId, 3));

			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 0), glm::vec2(1, 1), textureId, 3));
			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 1), glm::vec2(0, 1), textureId, 3));
			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 0), glm::vec2(1, 0), textureId, 3));
		}

		// Forward
		if (!checkSolidBlock(chunkGeneration, worldBlockCoord + FORWARD_DIR))
		{
			uint32_t textureId = getTextureID(block, BlockSide::SIDE);
			glm::ivec3 chunkBlockCoord = blockCoordToChunkBlockCoord(worldBlockCoord);

			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 1), glm::vec2(0, 0), textureId, 4));
			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 1), glm::vec2(1, 0), textureId, 4));
			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 1), glm::vec2(1, 1), textureId, 4));

			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 1), glm::vec2(1, 1), textureId, 4));
			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(1, 0, 1), glm::vec2(0, 1), textureId, 4));
			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 1), glm::vec2(0, 0), textureId, 4));
		}

		// Backward
		if (!checkSolidBlock(chunkGeneration, worldBlockCoord - FORWARD_DIR))
		{
			uint32_t textureId = getTextureID(block, BlockSide::SIDE);
			glm::ivec3 chunkBlockCoord = blockCoordToChunkBlockCoord(worldBlockCoord);

			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 0), glm::vec2(0, 1), textureId, 5));
			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 0), glm::vec2(0, 0), textureId, 5));
			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 0), glm::vec2(1, 0), textureId, 5));

			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 0), glm::vec2(1, 0), textureId, 5));
			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(1, 0, 0), glm::vec2(1, 1), textureId, 5));
			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 0), glm::vec2(0, 1), textureId, 5));
		}
	}

	void ChunkGenerator::addWaterMeshData(ChunkGenerationThread& chunkGeneration, const glm::ivec3& worldBlockCoord, MeshData& outMesh)
	{
		glm::ivec3 aboveBlockCoord = worldBlockCoord + UP_DIR;
		if (aboveBlockCoord.y >= WORLD_HEIGHT)
			return;

		glm::ivec3 aboveChunkBlockCoord = blockCoordToChunkBlockCoord(aboveBlockCoord);
		uint32_t aboveBlockIdx = chunkBlockCoordToChunkBlockIdx(aboveChunkBlockCoord);

		if (chunkGeneration.chunkData.blocks[aboveBlockIdx] != BlockType::WATER)
		{
			uint32_t textureId = getTextureID(BlockType::WATER, BlockSide::SIDE);
			glm::ivec3 chunkBlockCoord = blockCoordToChunkBlockCoord(worldBlockCoord);

			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 0), glm::vec2(1, 0), textureId));
			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 1), glm::vec2(1, 1), textureId));
			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 1), glm::vec2(0, 1), textureId));

			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 1), glm::vec2(0, 1), textureId));
			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 0), glm::vec2(0, 0), textureId));
			addVertex(outMesh, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 0), glm::vec2(1, 0), textureId));
		}
	}

	void ChunkGenerator::generateBlockData(ChunkGenerationThread& chunkGeneration)
	{
		const int loadWidth = 1;
		glm::ivec2 chunkCoord = chunkIDToChunkCoord(chunkGeneration.chunkID);

		for (int offsetX = -loadWidth; offsetX <= loadWidth; offsetX++)
		{
			for (int offsetZ = -loadWidth; offsetZ <= loadWidth; offsetZ++)
			{
				ChunkID adjacentChunkID = chunkCoordToChunkID(chunkCoord + glm::ivec2(offsetX, offsetZ));
				cacheStructures(chunkGeneration, adjacentChunkID);
			}
		}

		for (uint32_t i = 0; i < MAX_BLOCKS_IN_CHUNK; i++)
		{
			glm::ivec3 chunkBlockCoord = chunkBlockIdxToChunkBlockCoord(i);
			glm::ivec3 blockCoord = chunkBlockCoordToBlockCoord(chunkGeneration.chunkID, chunkBlockCoord);
			chunkGeneration.chunkData.blocks[i] = generateBlock(chunkGeneration, blockCoord);
		}
	}

	void ChunkGenerator::generateMeshData(ChunkGenerationThread& chunkGeneration, uint32_t meshGenID)
	{
		glm::ivec2 chunkCoord = chunkIDToChunkCoord(chunkGeneration.chunkID);
		glm::ivec3 worldCoord = chunkCoordToWorldCoord(chunkCoord);

		chunkGeneration.blockMesh.vertices.reserve(MAX_BLOCKS_IN_CHUNK * 36ull);
		chunkGeneration.blockMesh.indices.reserve(MAX_BLOCKS_IN_CHUNK * 36ull);

		chunkGeneration.waterMesh.vertices.reserve(MAX_BLOCKS_IN_CHUNK * 36ull / 2);
		chunkGeneration.waterMesh.indices.reserve(MAX_BLOCKS_IN_CHUNK * 36ull / 2);

		for (uint32_t i = 0; i < MAX_BLOCKS_IN_CHUNK; i++)
		{
			BlockType block = chunkGeneration.chunkData.blocks[i];
			if (block == BlockType::AIR)
				continue;

			glm::ivec3 chunkBlockCoord = chunkBlockIdxToChunkBlockCoord(i);
			glm::ivec3 worldBlockCoord = chunkBlockCoord + worldCoord;

			if (block == BlockType::WATER)
			{
				addWaterMeshData(chunkGeneration, worldBlockCoord, chunkGeneration.waterMesh);
			}
			else
			{
				addBlockMeshData(chunkGeneration, block, worldBlockCoord, chunkGeneration.blockMesh);
			}
		}
	}

	void ChunkGenerator::tryRemoveLastMeshGenID(ChunkID chunkID)
	{
		bool canRemoveGenID = true;
		for (auto& chunkIt2 : m_loadingChunks)
		{
			if (chunkIt2.second.chunkID == chunkID)
			{
				canRemoveGenID = false;
				break;
			}
		}

		if (canRemoveGenID)
			m_lastMeshGenIDs.erase(chunkID);
	}

	void ChunkGenerator::queueMeshUpdate(ChunkID chunkID)
	{
		if (!m_pWorld->isChunkLoaded(chunkID))
			return;


		ChunkGenID loadID = s_chunkGenID++;
		MeshGenID meshGenID = ++m_lastMeshGenIDs[chunkID];

		ChunkGenerationThread& chunkGen = m_loadingChunks[loadID];
		chunkGen.threadFinished = false;
		chunkGen.blocksGenerated = false;
		chunkGen.chunkID = chunkID;
		chunkGen.meshGenID = meshGenID;

		memcpy(chunkGen.chunkData.blocks, m_pWorld->getChunkConst(chunkID).blocks, sizeof(Chunk::blocks));

		glm::ivec2 chunkCoord = chunkIDToChunkCoord(chunkID);
		uint32_t refIdx = 0;
		for (const glm::ivec2& offset : ADJACENT_CHUNK_OFFSETS)
		{
			ChunkID adjacentChunkID = chunkCoordToChunkID(chunkCoord + offset);
			const Chunk* pAdjacentChunk = m_pWorld->tryGetChunk(adjacentChunkID);
			if (!pAdjacentChunk)
				continue;

			chunkGen.chunkRefs[refIdx].chunkID = adjacentChunkID;
			chunkGen.chunkRefs[refIdx].pChunk = pAdjacentChunk;
			pAdjacentChunk->meshReadRefCount++;

			refIdx++;
		}

		ChunkGenerationThread* pChunkGen = &m_loadingChunks[loadID];
		m_threadPool.queueJob([=]()
			{
				generateMeshData(*pChunkGen, 0);
				pChunkGen->threadFinished = true;
			});
	}

	void ChunkGenerator::handleMeshUpdates(ChunkID chunkID)
	{
		glm::ivec2 chunkCoord = chunkIDToChunkCoord(chunkID);

		m_chunkMeshUpdates[chunkID] = true;
		for (const glm::ivec2& offset : ADJACENT_CHUNK_OFFSETS)
		{
			ChunkID adjacentChunkID = chunkCoordToChunkID(chunkCoord + offset);

			if (m_loadingChunks.contains(adjacentChunkID) || m_pWorld->isChunkLoaded(adjacentChunkID))
				m_chunkMeshUpdates[adjacentChunkID] = true;
		}
	}
}
