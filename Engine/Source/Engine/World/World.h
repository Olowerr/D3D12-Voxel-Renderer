#pragma once

#include "Chunk.h"
#include "Camera.h"

#include <unordered_map>

namespace Okay
{
	class World
	{
	public:
		struct ChunkGeneration
		{
			std::thread genThread;
			std::atomic<bool> threadFinished;

			ChunkID chunkID = INVALID_CHUNK_ID;
			Chunk chunk;
		};

	public:
		World();
		~World();

		void update();

		uint8_t getBlockAtBlockCoord(const glm::ivec3& blockCoord) const;
		uint8_t tryGetBlock(ChunkID chunkID, uint32_t blockIdx) const;

		Chunk& getChunk(ChunkID chunkID);
		const Chunk& getChunkConst(ChunkID chunkID) const;

		const Chunk* tryGetChunk(ChunkID chunkID) const;
		bool isChunkLoaded(ChunkID chunkID) const;

		const std::vector<ChunkID>& getAddedChunks() const;
		const std::vector<ChunkID>& getRemovedChunks() const;

		Camera& getCamera();
		const Camera& getCameraConst() const;

	private:
		void launchChunkGenerationThread(ChunkID chunkID);

		void clearUpdatedChunks();
		bool isChunkWithinRenderDistance(const glm::ivec2& chunkCoord) const;

		bool isChunkLoading(ChunkID chunkID) const;

	private:
		Camera m_camera;
		glm::ivec2 m_currentCamChunkCoord = glm::ivec2(0, 0);

		std::unordered_map<ChunkID, Chunk> m_loadedChunks;
		std::unordered_map<ChunkID, ChunkGeneration> m_loadingChunks;

		std::vector<ChunkID> m_addedChunks;
		std::vector<ChunkID> m_removedChunks;
	};
}
