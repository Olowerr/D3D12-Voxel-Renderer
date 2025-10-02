#pragma once
#include "Engine/Okay.h"

namespace Random
{
	inline uint32_t pcgHash(uint32_t& seed)
	{
		seed *= 747796405u + 2891336453u;
		seed = ((seed >> ((seed >> 28u) + 4u)) ^ seed) * 277803737u;
		seed = (seed >> 22u) ^ seed;
		return seed;
	}

	inline float randomFloat(uint32_t& seed)
	{
		return pcgHash(seed) / (float)UINT_MAX;
	}
}
