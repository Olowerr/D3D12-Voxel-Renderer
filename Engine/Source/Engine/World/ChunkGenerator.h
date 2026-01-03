#pragma once

#include "Engine/World/Chunk.h"
#include "Engine/World/Structure.h"
#include "Engine/Utilities/ThreadPool.h"

#include <unordered_map>

namespace Okay
{
	using ChunkGenID = uint64_t;
	using MeshGenID = uint8_t;
	constexpr MeshGenID INVALID_MESH_GEN_ID = INVALID_UINT8;

	class World;
	struct Camera;

	struct ChunkGenerationThread
	{
		ChunkID chunkID = INVALID_CHUNK_ID;
		MeshGenID meshGenID = INVALID_MESH_GEN_ID;

		std::vector<Structure> structures;
		Chunk chunkData;
		MeshData blockMesh;
		MeshData waterMesh;

		std::atomic_bool threadFinished;
		std::atomic_bool blocksGenerated;
	};

	class ChunkGenerator
	{
	public:
		// temp location
		static bool isBlockTypeSolid(BlockType block);

	public:
		ChunkGenerator() = default;
		~ChunkGenerator() = default;

		void initialize(uint64_t seed, const BlockTextureIDs& textures, const World& world);
		void shutdown();

		void applySeed(uint64_t seed);

		void update(const Camera& camera);

		void queueChunkGeneration(ChunkID chunkID);
		const std::vector<ChunkID>& getCompletedChunks() const;
		const ChunkGenerationThread& getChunkGenData(ChunkGenID chunkGenID) const;

	private:
		uint32_t findColoumnHeight(const glm::ivec3& blockCoordXZ) const;
		bool shouldPlaceTree(const glm::ivec3& blockCoord) const;
		void cacheStructures(ChunkGenerationThread& chunkGeneration, ChunkID sourceChunkID);
		BlockType tryFindStructureBlock(ChunkGenerationThread& chunkGeneration, const glm::ivec3& blockCoord) const;
		BlockType searchChunkForStructure(ChunkGenerationThread& chunkGeneration, const glm::ivec3& blockCoord) const;
		BlockType generateBlock(ChunkGenerationThread& chunkGeneration, const glm::ivec3& blockCoord) const;

		bool checkSolidBlock(ChunkGenerationThread& chunkGeneration, const glm::ivec3& blockCoord);
		void addBlockMeshData(ChunkGenerationThread& chunkGeneration, BlockType block, const glm::ivec3& worldBlockCoord, MeshData& outMesh);
		void addWaterMeshData(ChunkGenerationThread& chunkGeneration, const glm::ivec3& worldBlockCoord, MeshData& outMesh);
		uint32_t getTextureID(BlockType blockType, BlockSide blockSide) const;

		void generateBlockData(ChunkGenerationThread& chunkGeneration);
		void generateMeshData(ChunkGenerationThread& chunkGeneration, uint32_t meshGenID);
		
		void tryRemoveLastMeshGenID(ChunkID chunkID);
		void queueMeshUpdate(ChunkID chunkID);
		void handleMeshUpdates(ChunkID chunkID);

		void processLoadingChunks();
		void tryLoadRenderEligableChunks(const Camera& camera);

	private:
		ThreadPool m_threadPool;

		std::vector<ChunkGenID> m_completedIDs;
		std::unordered_map<ChunkGenID, ChunkGenerationThread> m_loadingChunks;
		std::unordered_map<ChunkID, MeshGenID> m_lastMeshGenIDs;
		std::unordered_map<ChunkID, bool> m_chunkMeshUpdates;

		const BlockTextureIDs* m_pBlockTextureIds = nullptr; // sus?
		const World* m_pWorld = nullptr; // sus?
	};
}
