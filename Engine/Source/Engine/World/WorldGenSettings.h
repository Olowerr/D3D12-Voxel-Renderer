#pragma once
#include "Engine/Utilities/InterpolationList.h"
#include "Engine/Utilities/Noise.h"

#include "glm/glm.hpp"

#include <vector>

namespace Okay
{
	struct CloudGenerationData
	{
	private:
		CloudGenerationData() = default;
	public:
		~CloudGenerationData() = default;
		CloudGenerationData(const CloudGenerationData&) = delete;
		CloudGenerationData(CloudGenerationData&&) = delete;
		CloudGenerationData& operator=(const CloudGenerationData&) = delete;

		static inline CloudGenerationData& get()
		{
			static CloudGenerationData worldGenData;
			return worldGenData;
		}

		static const float UPDATE_INTERVAL;
		float updateTimer = UPDATE_INTERVAL;

		Noise::SamplingData cloudNoise;
		Noise::SamplingData maskNoise;

		glm::vec2 velocity = glm::vec2(1.f, 1.f);
		glm::vec2 localDrift = glm::vec2(FLT_MAX);
		glm::vec2 globalDrift = glm::vec2(0.f);

		uint32_t spawnHeight = 200;
		float scale = 9.f;
		float height = 100.f;
		float maxOffset = 6.f;
		float sampleDistance = 8.f;
		uint32_t chunkVisiblityDistance = 32;
		glm::vec4 colour = glm::vec4(248.f, 255.f, 255.f, 95.f) / (float)UCHAR_MAX;
	};

	struct WorldGenerationData
	{
	private:
		WorldGenerationData() = default;
	public:
		~WorldGenerationData() = default;
		WorldGenerationData(const WorldGenerationData&) = delete;
		WorldGenerationData(WorldGenerationData&&) = delete;
		WorldGenerationData& operator=(const WorldGenerationData&) = delete;

		static inline WorldGenerationData& get()
		{
			static WorldGenerationData worldGenData;
			return worldGenData;
		}

		uint32_t seed = 0;
		uint32_t renderDistance = 16;

		uint32_t oceanHeight = 70;
		float amplitude = 70.f;

		Noise::SamplingData terrainNoiseData;
		InterpolationList terrrainNoiseInterpolation = InterpolationList({ -1.f, -1.f }, { 1.f, 1.f });

		Noise::SamplingData treeAreaNoiseData;
		float treeAreaNoiseThreshold = 0.5f;

		Noise::SamplingData treeNoiseData;
		float treeThreshold = 0.46f;
		uint32_t treeMaxSpawnAltitude = 90;

		bool pauseGen = false;
	};
}
