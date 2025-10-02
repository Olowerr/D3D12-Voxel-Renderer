#include "Noise.h"

#include "glm/common.hpp"
#include "db_perlin/db_perlin.hpp"

namespace Okay
{
	namespace Noise
	{
		static float samplePerlin_Internal(float x, float y, const SamplingData& samplingData)
		{
			float frequency = samplingData.frequencyNumerator / samplingData.frequencyDenominator;
			x *= frequency;
			y *= frequency;

			float noise = db::perlin_octave2D(x, y, samplingData.numOctaves, samplingData.persistence);
			noise = noise * 0.5f + 0.5f;

			noise = noise >= samplingData.cutOff ? ((noise - samplingData.cutOff) / (1.f - samplingData.cutOff)) : 0.f;
			noise = glm::pow(noise, samplingData.exponent);

			return noise;
		}

		void applyPerlinSeed(uint32_t seed)
		{
			db::reseed(seed);
		}

		float samplePerlin2D_minusOneOne(float x, float y, const SamplingData& samplingData)
		{
			float noise = samplePerlin_Internal(x, y, samplingData);
			noise = noise * 2.f - 1.f;

			return noise;
		}

		float samplePerlin2D_zeroOne(float x, float y, const SamplingData& samplingData)
		{
			float noise = samplePerlin_Internal(x, y, samplingData);
			return noise;
		}
	}
}
