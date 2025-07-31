#pragma once
#include "Engine/Okay.h"

#include <unordered_map>
#include <string_view>

namespace Okay
{
	/*
		The "X" macro:
		https://digitalmars.com/articles/b51.html
		https://en.wikipedia.org/wiki/X_macro
		https://www.drdobbs.com/the-new-c-x-macros/184401387
	*/

#define BLOCKS_TABLE_MACRO \
X(AIR, 0) \
X(DIRT, 1) \
X(GRASS, 2) \
X(STONE, 3) \

#define X(name, value) name = value,
	enum struct BlockType : uint8_t
	{
		INVALID = INVALID_UINT8,
		BLOCKS_TABLE_MACRO
	};
#undef X

#define X(name, value) #name,
	static const char* BLOCK_NAMES[] =
	{
		BLOCKS_TABLE_MACRO
	};
#undef X

	constexpr uint32_t NUM_BLOCKS = _countof(BLOCK_NAMES);

	enum BlockSide : uint8_t // Needs to match searchSides array in findBlockTextures()
	{
		TOP = 0,
		SIDE = 1,
		BOTTOM = 2,
	};

	struct BlockTextures
	{
		std::string textures[3] = {};
	};
	void findBlockTextures(std::unordered_map<BlockType, BlockTextures>& outTextures);
}
