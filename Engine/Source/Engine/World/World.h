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

	struct CloudGenerationData
	{
		static const float UPDATE_INTERVAL;
		float updateTimer = UPDATE_INTERVAL;

		std::vector<glm::vec3> cloudList;

		Noise::SamplingData cloudNoise;
		Noise::SamplingData maskNoise;

		glm::vec2 velocity = glm::vec2(1.f, 1.f);
		glm::vec2 localDrift = glm::vec2(FLT_MAX);
		glm::vec2 globalDrift = glm::vec2(0.f);

		uint32_t spawnHeight = 200;
		float scale = 9.f;
		float height = 100.f;
		float maxOffset = 6.f;
		float sampleDistance = 8.f;
		uint32_t chunkVisiblityDistance = 32;
		glm::vec4 colour = glm::vec4(248.f, 255.f, 255.f, 95.f) / (float)UCHAR_MAX;
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

		void update(const Camera& camera, TimeStep dt);

		BlockType getBlockAtBlockCoord(const glm::ivec3& blockCoord) const;
		BlockType tryGetBlock(ChunkID chunkID, uint32_t blockIdx) const;

		static bool isBlockTypeSolid(BlockType block);
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

		void recreateClouds();
		const std::vector<glm::vec3>& getCloudList() const;

		WorldGenerationData m_worldGenData;
		CloudGenerationData m_cloudGenData;
		uint32_t m_renderDistance = 32;

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

		void updateClouds(const Camera& camera, TimeStep dt);
		void generateCloudList(const Camera& camera);
		void clearDistanceClouds(const Camera& camera);
		void sampleCloud(float x, float z);

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
