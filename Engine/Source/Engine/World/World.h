#pragma once

#include "Chunk.h"
#include "Engine/Utilities/InterpolationList.h"

#include <atomic>
#include <unordered_map>

namespace Okay
{
	struct ChunkGeneration
	{
		std::atomic<bool> threadFinished;
		std::atomic<bool> cancel;
		ChunkID chunkID = INVALID_CHUNK_ID;
		Chunk chunk;
	};

	struct WorldGenerationData
	{
		uint32_t seed = 0;
		uint32_t octaves = 4;
		float frequency = 1.f / 150.f;
		float persistance = 0.5f;
		float amplitude = 70.f;

		uint32_t oceanHeight = 70;

		InterpolationList noiseInterpolation = InterpolationList({ -1.f, -1.f }, { 1.f, 1.f });
	};

	class Window;
	struct Camera;

	class World
	{
	public:
		World();
		~World() = default;

		void update(const Camera& camera);

		BlockType getBlockAtBlockCoord(const glm::ivec3& blockCoord) const;
		BlockType tryGetBlock(ChunkID chunkID, uint32_t blockIdx) const;
		bool isBlockCoordSolid(const glm::ivec3& blockCoord) const;

		Chunk& getChunk(ChunkID chunkID);
		const Chunk& getChunkConst(ChunkID chunkID) const;

		const Chunk* tryGetChunk(ChunkID chunkID) const;
		bool isChunkLoaded(ChunkID chunkID) const;

		const std::vector<ChunkID>& getAddedChunks() const;
		const std::vector<ChunkID>& getRemovedChunks() const;

		void resetWorld();
		WorldGenerationData m_worldGenData;

	private:
		void launchChunkGenerationThread(ChunkID chunkID);
		void generateChunk(ChunkGeneration* pChunkGeneration);

		void clearUpdatedChunks();
		void unloadDistantChunks();
		void processLoadingChunks();
		void tryLoadRenderEligableChunks(const Camera& camera);

		bool isChunkWithinRenderDistance(ChunkID chunkID) const;
		bool isChunkLoading(ChunkID chunkID) const;

		bool isChunkInView(const Camera& camera, ChunkID chunkID) const;

	private:
		glm::ivec2 m_currentCamChunkCoord = glm::ivec2(0, 0);
		float m_aspectRatio = 0.f;

		std::unordered_map<ChunkID, Chunk> m_loadedChunks;
		std::unordered_map<ChunkID, ChunkGeneration> m_loadingChunks;

		std::vector<ChunkID> m_addedChunks;
		std::vector<ChunkID> m_removedChunks;

	};
}
