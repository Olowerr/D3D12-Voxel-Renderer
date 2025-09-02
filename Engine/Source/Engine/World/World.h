#pragma once

#include "Chunk.h"
#include "Engine/Utilities/InterpolationList.h"
#include "Engine/Utilities/Noise.h"
#include "Engine/Utilities/ThreadPool.h"
#include "Engine/Application/Time.h"
#include "Structure.h"

#include <atomic>
#include <unordered_map>

namespace Okay
{
	struct ChunkGeneration
	{
		std::atomic<bool> threadFinished;
		ChunkID chunkID = INVALID_CHUNK_ID;
		Chunk chunk;
	};

	struct WorldGenerationData
	{
		uint32_t seed = 0;
		uint32_t oceanHeight = 70;
		float amplitude = 70.f;

		Noise::SamplingData terrainNoiseData;
		InterpolationList terrrainNoiseInterpolation = InterpolationList({ -1.f, -1.f }, { 1.f, 1.f });

		Noise::SamplingData treeAreaNoiseData;
		float treeAreaNoiseThreshold = 0.5f;

		Noise::SamplingData treeNoiseData;
		float treeThreshold = 0.46f;
		uint32_t treeMaxSpawnAltitude = 90;
	};

	class Window;
	struct Camera;

	class World
	{
	public:
		World() = default;
		~World() = default;

		void initialize();
		void shutdown();

		void update(const Camera& camera);

		BlockType getBlockAtBlockCoord(const glm::ivec3& blockCoord) const;
		BlockType tryGetBlock(ChunkID chunkID, uint32_t blockIdx) const;
		bool isBlockCoordSolid(const glm::ivec3& blockCoord) const;

		BlockType generateBlock(const glm::ivec3& blockCoord);

		Chunk& getChunk(ChunkID chunkID);
		const Chunk& getChunkConst(ChunkID chunkID) const;

		const Chunk* tryGetChunk(ChunkID chunkID) const;
		bool isChunkLoaded(ChunkID chunkID) const;

		const std::vector<ChunkID>& getAddedChunks() const;
		const std::vector<ChunkID>& getRemovedChunks() const;

		void applySeed() const;
		void resetWorld();
		WorldGenerationData m_worldGenData;

	private:
		void launchChunkGenerationThread(ChunkID chunkID);
		void generateChunk(ChunkGeneration* pChunkGeneration);
		bool shouldPlaceTree(const glm::ivec3& blockCoordXZ) const;
		uint32_t findColoumnHeight(const glm::ivec3& blockCoordXZ);

		void loadChunkStructures(ChunkID chunkID);
		BlockType searchChunkForStructure(ChunkID chunkID, const glm::ivec3& blockCoord) const;
		BlockType tryFindStructureBlock(const glm::ivec3& blockCoord) const;
		
		void clearUpdatedChunks();
		void unloadDistantChunks();
		void processLoadingChunks();
		void tryLoadRenderEligableChunks(const Camera& camera);

		bool isChunkWithinRenderDistance(ChunkID chunkID) const;
		bool isChunkLoading(ChunkID chunkID) const;

		bool isChunkInView(const Camera& camera, ChunkID chunkID) const;

	private:
		ThreadPool m_threadPool;

		glm::ivec2 m_currentCamChunkCoord = glm::ivec2(0, 0);
		float m_aspectRatio = 0.f;

		std::unordered_map<ChunkID, Chunk> m_loadedChunks;
		std::unordered_map<ChunkID, ChunkGeneration> m_loadingChunks;
		std::unordered_map<ChunkID, ChunkStructures> m_chunksStructures;

		std::vector<ChunkID> m_addedChunks;
		std::vector<ChunkID> m_removedChunks;

	};
}
