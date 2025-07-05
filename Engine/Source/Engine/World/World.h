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

		bool isChunkBlockCoordOccupied(const glm::ivec3& blockCoord) const;
		
		Chunk& getChunk(ChunkID chunkID);
		const Chunk& getChunkConst(ChunkID chunkID) const;

		const Chunk* tryGetChunk(ChunkID chunkID) const;
		bool isChunkLoaded(ChunkID chunkID) const;

		void clearNewChunks();
		const std::vector<ChunkID>& getNewChunks() const;

		Camera& getCamera();
		const Camera& getCameraConst() const;

	private:
		void generateChunk(const glm::ivec2& worldPos);

	private:
		Camera m_camera;

		// TODO: Compare performance using std::unordered_map & std::vector for the chunks
		std::unordered_map<ChunkID, Chunk> m_chunks;
		std::vector<ChunkID> m_newChunks;
	};
}
