#pragma once

#include "glm/glm.hpp"

#include <inttypes.h>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <algorithm>

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

	inline const FilePath RESOURCES_PATH = FilePath("..") / "Engine" / "resources";
	inline const FilePath TEXTURES_PATH = RESOURCES_PATH / "textures";

	constexpr uint8_t  INVALID_UINT8  = UINT8_MAX;
	constexpr uint16_t INVALID_UINT16 = UINT16_MAX;
	constexpr uint32_t INVALID_UINT32 = UINT32_MAX;
	constexpr uint64_t INVALID_UINT64 = UINT64_MAX;

	constexpr glm::ivec3 RIGHT_DIR = glm::ivec3(1, 0, 0);
	constexpr glm::ivec3 UP_DIR = glm::ivec3(0, 1, 0);
	constexpr glm::ivec3 FORWARD_DIR = glm::ivec3(0, 0, 1);

	struct Vertex
	{
		Vertex() = default;

		// Blocks
		Vertex(const glm::ivec3& position, const glm::vec2& globalUV, uint32_t textureID, uint32_t sideIdx)
		{
			data = 0;
			writeBits(position.x, 0, 5);
			writeBits(position.y, 5, 9);
			writeBits(position.z, 14, 5);

			writeBits((uint32_t)globalUV.x, 19, 1);
			writeBits((uint32_t)globalUV.y, 20, 1);
			writeBits(textureID, 21, 8);
			writeBits(sideIdx, 29, 3);
		}

		// Water
		Vertex(const glm::ivec3& position, const glm::vec2& globalUV, uint32_t textureID)
		{
			data = 0;
			writeBits(position.x, 0, 5);
			writeBits(position.y, 5, 9);
			writeBits(position.z, 14, 5);

			writeBits((uint32_t)globalUV.x, 19, 1);
			writeBits((uint32_t)globalUV.y, 20, 1);

			// Kinda unneccessary to store this, option is to store it in cbuffer, but feels meh
			writeBits(textureID, 21, 8);
		}

		void writeBits(uint32_t value, uint32_t bitPos, uint32_t numBits)
		{
			data |= value << (32 - (bitPos + numBits));
		}

		bool operator==(Vertex other) const
		{
			return data == other.data;
		}

		uint32_t data = INVALID_UINT32;
	};

	struct MeshData
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
	};

	inline bool readBinary(const FilePath& binPath, std::string& output)
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

	inline std::string strToLowerCase(const std::string& string)
	{
		// https://stackoverflow.com/questions/313970/how-to-convert-an-instance-of-stdstring-to-lower-case

		std::string result = string;
		std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c)
			{
				return std::tolower(c);
			});

		return std::move(result);
	}
}
