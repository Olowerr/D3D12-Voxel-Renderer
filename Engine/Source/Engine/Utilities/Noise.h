#pragma once

#include "Engine/Okay.h"

namespace Okay
{
	namespace Noise
	{
		struct SamplingData
		{
			uint32_t numOctaves = 1;
			float frequencyNumerator = 1.f;
			float frequencyDenominator = 1.f;
			float persistence = 0.5f;
			float exponent = 1.f;
			float cutOff = 0.f;
		};

		void applyPerlinSeed(uint32_t seed);
		float samplePerlin2D_minusOneOne(float x, float y, const SamplingData& samplingData);
		float samplePerlin2D_zeroOne(float x, float y, const SamplingData& samplingData);
	}
}
