#include "Blocks.h"

namespace Okay
{
	void findBlockTextures(std::unordered_map<BlockType, BlockTextures>& outTextures)
	{
		std::string searchSides[] = // Needs to match BlockSide Enum
		{
			"top",
			"side",
			"bottom",
		};


		// Go through the blocks and find textures for the sides of the blocks
		for (uint32_t i = 1; i < NUM_BLOCKS; i++)
		{
			BlockType blockType = BlockType(i);
			std::string blockName = strToLowerCase(BLOCK_NAMES[i]);

			if (std::filesystem::exists(TEXTURES_PATH / (std::string(blockName) + ".png")))
			{
				for (uint32_t k = 0; k < 3; k++)
				{
					outTextures[blockType].textures[k] = blockName;
				}
			}

			for (uint32_t k = 0; k < 3; k++)
			{
				std::string textureName = blockName + "_" + searchSides[k];
				if (std::filesystem::exists(TEXTURES_PATH / (textureName + ".png")))
				{
					outTextures[blockType].textures[k] = textureName;
				}
			}
		}


		// Manually set some textures so we can reuse them
		outTextures[BlockType::GRASS].textures[BlockSide::BOTTOM] = "dirt";
		outTextures[BlockType::OAK_LOG].textures[BlockSide::BOTTOM] = "oak_log_top";


		// Ensure every side of every block has a texture
		for (uint32_t i = 1; i < NUM_BLOCKS; i++)
		{
			auto iterator = outTextures.find(BlockType(i));
			OKAY_ASSERT(iterator != outTextures.end());

			for (const std::string& texture : iterator->second.textures)
			{
				OKAY_ASSERT(!texture.empty());
			}
		}
	}
}
