#include "Blocks.h"

namespace Okay
{
	void findBlockTextures(std::unordered_map<BlockType, SideTextureNames>& outTextures)
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

			if (std::filesystem::exists(TEXTURES_PATH / (blockName + ".png")))
			{
				for (uint32_t k = 0; k < 3; k++)
				{
					outTextures[blockType].names[k] = blockName;
				}
			}

			for (uint32_t k = 0; k < 3; k++)
			{
				std::string textureName = blockName + "_" + searchSides[k];
				if (std::filesystem::exists(TEXTURES_PATH / (textureName + ".png")))
				{
					outTextures[blockType].names[k] = textureName;
				}
			}
		}


		// Manually set some textures so we can reuse them
		outTextures[BlockType::GRASS].names[BlockSide::BOTTOM] = "dirt";
		outTextures[BlockType::OAK_LOG].names[BlockSide::BOTTOM] = "oak_log_top";


		// Ensure every side of every block has a texture
		for (uint32_t i = 1; i < NUM_BLOCKS; i++)
		{
			auto iterator = outTextures.find(BlockType(i));
			OKAY_ASSERT(iterator != outTextures.end());

			for (const std::string& texture : iterator->second.names)
			{
				OKAY_ASSERT(!texture.empty());
			}
		}
	}
}
