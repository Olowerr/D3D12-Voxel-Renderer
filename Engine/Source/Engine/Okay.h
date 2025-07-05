#pragma once

#include "glm/glm.hpp"

#include <inttypes.h>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>

// Will be defined to not check in dist builds
#define OKAY_ASSERT(condition)																	\
	{																							\
		if (!(condition))																		\
		{																						\
			printf("ASSERT FAILED: %s\nFile: %s\nLine: %d\n", #condition, __FILE__, __LINE__);	\
			__debugbreak();																		\
		}																						\
	}0

namespace Okay
{
	typedef std::filesystem::path FilePath;

	constexpr uint8_t INVALID_UINT8 = UINT8_MAX;
	constexpr uint16_t INVALID_UINT16 = UINT16_MAX;
	constexpr uint32_t INVALID_UINT32 = UINT32_MAX;
	constexpr uint64_t INVALID_UINT64 = UINT64_MAX;

	constexpr uint32_t WORLD_HEIGHT = 256;
	constexpr uint32_t CHUNK_WIDTH = 16;
	constexpr uint32_t MAX_BLOCKS_IN_CHUNK = CHUNK_WIDTH * CHUNK_WIDTH * WORLD_HEIGHT;

	constexpr uint32_t WORLD_CHUNK_WIDTH = 100; // How many chunks the world can have in X & Z directions (100 temp)

	constexpr glm::uvec3 RIGHT_DIR = glm::uvec3(1, 0, 0);
	constexpr glm::uvec3 UP_DIR = glm::uvec3(0, 1, 0);
	constexpr glm::uvec3 FORWARD_DIR = glm::uvec3(0, 0, 1);

	typedef uint64_t ChunkID;

	struct Chunk // Chunk block coordinate system order: X -> Y -> Z
	{
		uint32_t blocks[MAX_BLOCKS_IN_CHUNK] = {};
	};

	constexpr uint32_t localChunkCoordToBlockIdx(const glm::uvec3& localChunkCoord)
	{
		return localChunkCoord.x + localChunkCoord.y * CHUNK_WIDTH + localChunkCoord.z * CHUNK_WIDTH * WORLD_HEIGHT;
	}

	constexpr glm::uvec3 blockIdxToLocalChunkCoord(uint32_t blockIdx)
	{
		glm::uvec3 localChunkCoord = {};
		localChunkCoord.x = blockIdx % CHUNK_WIDTH;
		localChunkCoord.y = (blockIdx / CHUNK_WIDTH) % WORLD_HEIGHT;
		localChunkCoord.z = blockIdx / (CHUNK_WIDTH * WORLD_HEIGHT);

		return localChunkCoord;
	}

	constexpr bool isLocalCoordInsideChunk(const glm::uvec3& localChunkCoord)
	{
		return localChunkCoord.x < CHUNK_WIDTH && localChunkCoord.y < WORLD_HEIGHT && localChunkCoord.z < CHUNK_WIDTH;
	}

	constexpr bool isChunkCoordOccupied(const Chunk& chunk, const glm::uvec3& coord)
	{
		return isLocalCoordInsideChunk(coord) && chunk.blocks[localChunkCoordToBlockIdx(coord)] != 0;
	}

	constexpr ChunkID chunkPosToChunkID(const glm::ivec2& chunkWorldPos)
	{
		// When I get chunk at 0,0 I want it to be in middle ish, not at the edge of the map
		uint32_t worldMiddleOffset = WORLD_CHUNK_WIDTH / 2;
		return ((ChunkID)chunkWorldPos.x + worldMiddleOffset) + ((ChunkID)chunkWorldPos.y + worldMiddleOffset) * WORLD_CHUNK_WIDTH;
	}

	constexpr glm::ivec2 chunkIDToChunkPos(ChunkID chunkID)
	{
		glm::ivec2 worldPos = {};
		worldPos.x = int32_t(chunkID % WORLD_CHUNK_WIDTH);
		worldPos.y = int32_t(chunkID / WORLD_CHUNK_WIDTH);

		return worldPos - glm::ivec2(WORLD_CHUNK_WIDTH / 2);
	}

	constexpr glm::vec3 chunkPosToWorldPos(const glm::ivec2& chunkWorldPos)
	{
		glm::vec3 worldPos = glm::vec3(chunkWorldPos.x, 0.f, chunkWorldPos.y);
		worldPos *= (float)CHUNK_WIDTH;

		return worldPos;
	}

	inline bool readBinary(FilePath binPath, std::string& output)
	{
		std::ifstream reader(binPath.c_str(), std::ios::binary);
		if (!reader)
		{
			return false;
		}

		reader.seekg(0, std::ios::end);
		output.reserve((size_t)reader.tellg());
		reader.seekg(0, std::ios::beg);

		output.assign(std::istreambuf_iterator<char>(reader), std::istreambuf_iterator<char>());

		return true;
	}
}
