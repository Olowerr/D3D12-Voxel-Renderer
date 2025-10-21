#include "Random.h"
#include <random>

namespace Random
{
	static std::uniform_real_distribution<float> s_randomFloats(0.0, 1.0);
	static std::default_random_engine s_generator;

	float getFloat()
	{
		return s_randomFloats(s_generator);
	}
}
