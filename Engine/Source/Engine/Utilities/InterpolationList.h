#pragma once
#include "Engine/Okay.h"

#include <vector>

namespace Okay
{
	class InterpolationList
	{
	public:
		struct ListPoint
		{
			ListPoint() = default;
			ListPoint(float position, float value)
				:position(position), value(value)
			{ }

			float position = 0.f;
			float value = 0.f;
		};

	public:
		InterpolationList();
		InterpolationList(ListPoint start, ListPoint end);

		void addPoint(float position, float value);
		float sample(float position);

		const std::vector<ListPoint>& getPoints() const;
		void updatePoint(uint64_t index, float position, float value);

	private:
		uint64_t findPositionIdx(float position);

	private:
		std::vector<ListPoint> m_points;

	};
}
