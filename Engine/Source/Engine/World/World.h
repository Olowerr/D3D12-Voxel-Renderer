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
		
		Chunk& getChunk(ChunkID chunkID);
		const Chunk& getChunkConst(ChunkID chunkID) const;

		const Chunk* tryGetChunk(ChunkID chunkID) const;
		bool isChunkLoaded(ChunkID chunkID) const;

		const std::vector<ChunkID>& getNewChunks() const;
		const std::vector<ChunkID>& getRemovedChunks() const;

		Camera& getCamera();
		const Camera& getCameraConst() const;

	private:
		void generateChunk(ChunkID chunkID);
		void generateChunkIfNotLoaded(ChunkID chunkID);

		void unloadChunk(ChunkID chunkID);
		void unloadChunkIfLoaded(ChunkID chunkID);

		void clearUpdatedChunks();
		bool isChunkWithinRenderDistance(const glm::ivec2& chunkCoord);

	private:
		Camera m_camera;
		glm::ivec2 m_currentCamChunkCoord;

		// TODO: Compare performance using std::unordered_map & std::vector for the chunks
		std::unordered_map<ChunkID, Chunk> m_chunks;

		std::vector<ChunkID> m_newChunks;
		std::vector<ChunkID> m_removedChunks;
	};
}
