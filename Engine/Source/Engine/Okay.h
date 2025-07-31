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
