#pragma once

#include "Engine/Okay.h"
#include "Camera.h"

namespace Okay
{
	constexpr uint32_t WORLD_HEIGHT = 256;
	constexpr uint32_t CHUNK_WIDTH = 16;
	constexpr uint32_t MAX_BLOCKS_IN_CHUNK = CHUNK_WIDTH * CHUNK_WIDTH * WORLD_HEIGHT;

	constexpr glm::uvec3 RIGHT_DIR = glm::uvec3(1, 0, 0);
	constexpr glm::uvec3 UP_DIR = glm::uvec3(0, 1, 0);
	constexpr glm::uvec3 FORWARD_DIR = glm::uvec3(0, 0, 1);

	struct Chunk // Chunk block coordinate system order: X -> Y -> Z
	{
		uint32_t blocks[MAX_BLOCKS_IN_CHUNK] = {};

		static uint32_t chunkCoordToBlockIdx(const glm::uvec3& chunkCoord)
		{
			return chunkCoord.x + chunkCoord.y * CHUNK_WIDTH + chunkCoord.z * CHUNK_WIDTH * WORLD_HEIGHT;
		}

		static glm::uvec3 blockIdxToChunkCoord(uint32_t i)
		{
			glm::uvec3 chunkCoord = {};
			chunkCoord.x = i % CHUNK_WIDTH;
			chunkCoord.y = (i / CHUNK_WIDTH) % WORLD_HEIGHT;
			chunkCoord.z = i / (CHUNK_WIDTH * WORLD_HEIGHT);

			return chunkCoord;
		}

		static bool isCoordInsideChunk(const glm::uvec3& chunkCoord)
		{
			return chunkCoord.x < CHUNK_WIDTH && chunkCoord.y < WORLD_HEIGHT && chunkCoord.z < CHUNK_WIDTH;
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
