#pragma once

#include "Engine/Okay.h"
#include "Camera.h"

namespace Okay
{
	constexpr uint32_t WORLD_HEIGHT = 16;
	constexpr uint32_t CHUNK_WIDTH = 16;
	constexpr uint32_t MAX_BLOCKS_IN_CHUNK = CHUNK_WIDTH * CHUNK_WIDTH * WORLD_HEIGHT;

	struct Chunk
	{
		// Chunk block coordinate system order: X -> Y -> Z

		uint32_t blocks[MAX_BLOCKS_IN_CHUNK] = {};

		static uint32_t chunkCoordToBlockIdx(const glm::uvec3& blockCoord)
		{
			return blockCoord.x + blockCoord.y * CHUNK_WIDTH + blockCoord.z * CHUNK_WIDTH * WORLD_HEIGHT;
		}

		static glm::uvec3 blockIdxToChunkCoord(uint32_t i)
		{
			glm::uvec3 chunkCoord = {};
			chunkCoord.x = i % CHUNK_WIDTH;
			chunkCoord.y = (i / CHUNK_WIDTH) % WORLD_HEIGHT;
			chunkCoord.z = i / (CHUNK_WIDTH * WORLD_HEIGHT);

			return chunkCoord;
		}
	};

	class World
	{
	public:
		World();
		~World();

		Camera& getCamera();
		const Camera& getCameraConst() const;

		const Chunk& getChunkConst() const;

	private:
		Camera m_camera;

		Chunk m_testChunk;
	};
}
