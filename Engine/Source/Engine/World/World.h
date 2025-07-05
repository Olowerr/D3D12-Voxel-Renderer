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

		Camera& getCamera();
		const Camera& getCameraConst() const;

		Chunk& getChunk(ChunkID chunkId);
		const Chunk& getChunkConst(ChunkID chunkId) const;

		void clearNewChunks();
		const std::vector<ChunkID>& getNewChunks() const;

	private:
		void generateChunk(const glm::ivec2& worldPos);

	private:
		Camera m_camera;

		// TODO: Compare performance using std::unordered_map & std::vector for the chunks
		std::unordered_map<ChunkID, Chunk> m_chunks;
		std::vector<ChunkID> m_newChunks;
	};
}
