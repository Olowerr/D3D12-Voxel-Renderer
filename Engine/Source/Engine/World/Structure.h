#pragma once

#include "Engine/Okay.h"
#include "Blocks.h"

namespace Okay
{
	constexpr uint32_t MAX_CHUNK_STRUCTURES = 10;

	enum class StructureType
	{
		NONE = 0,
		TREE,
	};

	struct Structure
	{
		Structure() = default;

		inline bool isWithinBounds(const glm::ivec3& blockCoord) const
		{
			return blockCoord.x >= worldBoundsMin.x && blockCoord.y >= worldBoundsMin.y && blockCoord.z >= worldBoundsMin.z &&
				blockCoord.x <= worldBoundsMax.x && blockCoord.y <= worldBoundsMax.y && blockCoord.z <= worldBoundsMax.z;
		}

		StructureType type = StructureType::NONE;
		glm::ivec3 worldBoundsMin = glm::ivec3(0);
		glm::ivec3 worldBoundsMax = glm::ivec3(0);
	};

	struct ChunkStructures
	{
		ChunkStructures() = default;

		Structure structures[MAX_CHUNK_STRUCTURES] = {};
		uint32_t numStructures = 0;
	};

	struct BlockDescription
	{
		BlockDescription() = default;
		BlockDescription(BlockType type, const glm::ivec3& position)
			:type(type), position(position)
		{
		}

		BlockType type = BlockType::INVALID;
		glm::ivec3 position = glm::ivec3(0);
	};

	struct StructureDescription
	{
		StructureDescription() = default;

		inline void findBounds()
		{
			boundsMax = glm::ivec3(0);
			for (const BlockDescription& blockDesc : blocks)
			{
				boundsMax = glm::max(boundsMax, blockDesc.position);
			}
		}

		// Assums no negative coordinates, (0, 0, 0) is min allowed value (boundsMin is 0)
		std::vector<BlockDescription> blocks;
		glm::ivec3 boundsMax = glm::ivec3(0);
	};

	inline StructureDescription createTreeDescription()
	{
		StructureDescription treeDesc;
		//treeDesc.blocks.emplace_back(BlockType::STONE, glm::ivec3(0, 0, 0));
		//treeDesc.blocks.emplace_back(BlockType::STONE, glm::ivec3(0, 1, 0));
		//treeDesc.blocks.emplace_back(BlockType::STONE, glm::ivec3(0, 2, 0));
		//treeDesc.blocks.emplace_back(BlockType::STONE, glm::ivec3(0, 3, 0));
		//treeDesc.blocks.emplace_back(BlockType::STONE, glm::ivec3(0, 4, 0));

		treeDesc.blocks.emplace_back(BlockType::OAK_LOG, glm::ivec3(2, 0, 2));
		treeDesc.blocks.emplace_back(BlockType::OAK_LOG, glm::ivec3(2, 1, 2));
		treeDesc.blocks.emplace_back(BlockType::OAK_LOG, glm::ivec3(2, 2, 2));
		treeDesc.blocks.emplace_back(BlockType::OAK_LOG, glm::ivec3(2, 3, 2));
		treeDesc.blocks.emplace_back(BlockType::OAK_LOG, glm::ivec3(2, 4, 2));

		// Base of leaves
		for (uint32_t x = 0; x < 5; x++)
		{
			for (uint32_t z = 0; z < 5; z++)
			{
				if (x == 2 && z == 2)
					continue;

				for (uint32_t y = 2; y < 5; y++)
				{
					treeDesc.blocks.emplace_back(BlockType::OAK_LEAVES, glm::ivec3(x, y, z));
				}
			}
		}

		// Top of leaves
		for (uint32_t x = 1; x < 4; x++)
		{
			for (uint32_t z = 1; z < 4; z++)
			{
				for (uint32_t y = 5; y < 7; y++)
				{
					treeDesc.blocks.emplace_back(BlockType::OAK_LEAVES, glm::ivec3(x, y, z));
				}
			}
		}

		treeDesc.findBounds();

		return treeDesc;
	}
}
