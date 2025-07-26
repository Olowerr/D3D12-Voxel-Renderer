#pragma once
#include "Engine/Okay.h"

#include <thread>
#include <atomic>
#include <vector>

namespace Okay
{
	typedef uint64_t ChunkID;
	constexpr ChunkID INVALID_CHUNK_ID = UINT64_MAX;

	constexpr uint32_t WORLD_HEIGHT = 256; // Has to be even
	constexpr uint32_t CHUNK_WIDTH = 16;   // Has to be even
	constexpr uint32_t MAX_BLOCKS_IN_CHUNK = CHUNK_WIDTH * CHUNK_WIDTH * WORLD_HEIGHT;

	constexpr uint32_t WORLD_CHUNK_WIDTH = 1'000'000; // How many chunks the world can have in X & Z directions, has to be even

	struct Chunk // Chunk block coordinate system order: X -> Y -> Z
	{
		Chunk()
		{
			for (uint8_t& block : blocks)
				block = 0;
		}
		uint8_t blocks[MAX_BLOCKS_IN_CHUNK] = {};
	};

	constexpr uint32_t chunkBlockCoordToChunkBlockIdx(const glm::ivec3& chunkBlockCoord)
	{
		return chunkBlockCoord.x + chunkBlockCoord.y * (int32_t)CHUNK_WIDTH + chunkBlockCoord.z * (int32_t)CHUNK_WIDTH * (int32_t)WORLD_HEIGHT;
	}

	constexpr glm::ivec3 chunkBlockIdxToChunkBlockCoord(uint32_t chunkBlockIdx)
	{
		glm::ivec3 chunkBlockCoord = {};
		chunkBlockCoord.x = chunkBlockIdx % CHUNK_WIDTH;
		chunkBlockCoord.y = (chunkBlockIdx / CHUNK_WIDTH) % WORLD_HEIGHT;
		chunkBlockCoord.z = chunkBlockIdx / (CHUNK_WIDTH * WORLD_HEIGHT);

		return chunkBlockCoord;
	}

	constexpr ChunkID chunkCoordToChunkID(const glm::ivec2& chunkCoord)
	{
		// When I get chunk at 0,0 I want it to be in the middle ish, not at the edge of the map
		uint32_t worldMiddleOffset = WORLD_CHUNK_WIDTH / 2;
		return ((ChunkID)chunkCoord.x + worldMiddleOffset) + ((ChunkID)chunkCoord.y + worldMiddleOffset) * WORLD_CHUNK_WIDTH;
	}

	constexpr glm::ivec2 chunkIDToChunkCoord(ChunkID chunkID)
	{
		glm::ivec2 chunkCoord = {};
		chunkCoord.x = int32_t(chunkID % WORLD_CHUNK_WIDTH);
		chunkCoord.y = int32_t(chunkID / WORLD_CHUNK_WIDTH);

		return chunkCoord - glm::ivec2(WORLD_CHUNK_WIDTH / 2);
	}

	constexpr glm::ivec3 chunkCoordToWorldCoord(const glm::ivec2& chunkCoord)
	{
		glm::ivec3 worldCoord = glm::ivec3(chunkCoord.x, 0, chunkCoord.y);
		worldCoord *= CHUNK_WIDTH;

		return worldCoord;
	}

	inline ChunkID blockCoordToChunkID(const glm::ivec3& blockCoord)
	{
		// glm::floor isn't constexpr grr (not like these functions can be called at compile time anyway)
		glm::ivec2 chunkCoord = glm::floor(glm::vec2(blockCoord.x / (float)CHUNK_WIDTH, blockCoord.z / (float)CHUNK_WIDTH));
		return chunkCoordToChunkID(chunkCoord);
	}

	inline glm::ivec3 blockCoordToChunkBlockCoord(const glm::ivec3& blockCoord)
	{
		// This function would've been (x = blockCoord.x % CHUNK_WIDTH) if blockCount was unsigned
		// but we gotta handle negative coordinates too, and I don't wanna use an if statement or ternary operator :eyes:
		glm::ivec3 chunkBlockCoord = {};
		chunkBlockCoord.x = (int32_t((glm::ceil(glm::abs(blockCoord.x) / (float)CHUNK_WIDTH)) * CHUNK_WIDTH) + blockCoord.x) % CHUNK_WIDTH;
		chunkBlockCoord.y = blockCoord.y;
		chunkBlockCoord.z = (int32_t((glm::ceil(glm::abs(blockCoord.z) / (float)CHUNK_WIDTH)) * CHUNK_WIDTH) + blockCoord.z) % CHUNK_WIDTH;;

		return chunkBlockCoord;
	}

	constexpr glm::ivec3 chunkBlockCoordToBlockCoord(ChunkID chunkID, const glm::ivec3& chunkBlockCoord)
	{
		glm::ivec3 chunkWorldCoord = chunkCoordToWorldCoord(chunkIDToChunkCoord(chunkID));
		return chunkWorldCoord + chunkBlockCoord;
	}
}
