#pragma once

#include "Engine/Okay.h"
#include "Camera.h"

#include <unordered_map>

namespace Okay
{
	class World
	{
	public:
		World();
		~World();

		void update();

		bool isChunkBlockCoordOccupied(const glm::ivec3& blockCoord) const;
		uint8_t tryGetBlock(ChunkID chunkID, uint32_t blockIdx) const;

		Chunk& getChunk(ChunkID chunkID);
		const Chunk& getChunkConst(ChunkID chunkID) const;

		const Chunk* tryGetChunk(ChunkID chunkID) const;
		bool isChunkLoaded(ChunkID chunkID) const;

		ChunkID getNewlyLoadedChunk() const;
		const std::vector<ChunkID>& getRemovedChunks() const;

		Camera& getCamera();
		const Camera& getCameraConst() const;

	private:
		void generateChunk(ChunkID chunkID);

		void clearUpdatedChunks();
		bool isChunkWithinRenderDistance(const glm::ivec2& chunkCoord) const;

	private:
		Camera m_camera;
		glm::ivec2 m_currentCamChunkCoord = glm::ivec2(0, 0);

		// TODO: Compare performance using std::unordered_map & std::vector for the chunks
		std::unordered_map<ChunkID, Chunk> m_chunks;

		ChunkID m_newlyLoadedChunk = INVALID_CHUNK_ID;
		std::vector<ChunkID> m_removedChunks;
	};
}
