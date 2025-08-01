#pragma once

#include "Chunk.h"
#include "Camera.h"

#include <atomic>
#include <unordered_map>

namespace Okay
{
	class World
	{
	public:
		struct ChunkGeneration
		{
			std::atomic<bool> threadFinished;
			std::atomic<bool> cancel;
			ChunkID chunkID = INVALID_CHUNK_ID;
			Chunk chunk;
		};

	public:
		World();
		~World();

		void update();

		BlockType getBlockAtBlockCoord(const glm::ivec3& blockCoord) const;
		BlockType tryGetBlock(ChunkID chunkID, uint32_t blockIdx) const;

		Chunk& getChunk(ChunkID chunkID);
		const Chunk& getChunkConst(ChunkID chunkID) const;

		const Chunk* tryGetChunk(ChunkID chunkID) const;
		bool isChunkLoaded(ChunkID chunkID) const;

		const std::vector<ChunkID>& getAddedChunks() const;
		const std::vector<ChunkID>& getRemovedChunks() const;

		Camera& getCamera();
		const Camera& getCameraConst() const;

		void setWorldGenFrequency(float frequency);
		void setWorldGenPersistance(float persistance);
		void setWorldGenAmplitude(float amplitude);
		void setWorldGenOctaves(uint32_t octaves);
		void setWorldGenSeed(uint32_t seed);
		void reloadWorld();

		float getWorldGenFrequency() const;
		float getWorldGenPersistance() const;
		float getWorldGenAmplitude() const;
		uint32_t getWorldGenOctaves() const;
		uint32_t getWorldGenSeed() const;

	private:
		void launchChunkGenerationThread(ChunkID chunkID);
		void generateChunk(ChunkGeneration* pChunkGeneration);

		void clearUpdatedChunks();
		void unloadDistantChunks();
		void processLoadingChunks();
		void tryLoadRenderEligableChunks();

		bool isChunkWithinRenderDistance(ChunkID chunkID) const;
		bool isChunkLoading(ChunkID chunkID) const;

	private:
		Camera m_camera;
		glm::ivec2 m_currentCamChunkCoord = glm::ivec2(0, 0);

		std::unordered_map<ChunkID, Chunk> m_loadedChunks;
		std::unordered_map<ChunkID, ChunkGeneration> m_loadingChunks;

		std::vector<ChunkID> m_addedChunks;
		std::vector<ChunkID> m_removedChunks;

		// Make into struct
		float m_worldGenFrequency = 0.01f;
		float m_worldGenPersistance = 0.5f;
		float m_worldGenAmplitude = 200.f;
		uint32_t m_worldGenOctaves = 4;
		uint32_t m_worldGenSeed = 0;
	};
}
