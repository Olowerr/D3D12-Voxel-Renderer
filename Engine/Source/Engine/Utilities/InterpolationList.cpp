#include "InterpolationList.h"

namespace Okay
{
	InterpolationList::InterpolationList()
	{
		m_points.reserve(2);
		m_points.emplace_back(0.f, 0.f);
		m_points.emplace_back(1.f, 1.f);
	}

	InterpolationList::InterpolationList(ListPoint start, ListPoint end)
	{
		m_points.reserve(2);
		m_points.emplace_back(start);
		m_points.emplace_back(end);
	}

	void InterpolationList::addPoint(float position, float value)
	{
		uint64_t index = findPositionIdx(position);
		m_points.emplace(m_points.begin() + index + 1, position, value);
	}

	float InterpolationList::sample(float position)
	{
		uint64_t firstSampleIdx = findPositionIdx(position);
		ListPoint& firstSamplePoint = m_points[firstSampleIdx];
		ListPoint& secondSamplePoint = m_points[firstSampleIdx + 1];

		float alpha = (position - firstSamplePoint.position) / (secondSamplePoint.position - firstSamplePoint.position);
		return glm::mix(firstSamplePoint.value, secondSamplePoint.value, glm::smoothstep(0.f, 1.f, alpha));
	}

	const std::vector<InterpolationList::ListPoint>& InterpolationList::getPoints() const
	{
		return m_points;
	}

	void InterpolationList::updatePoint(uint64_t index, float position, float value)
	{
		m_points[index] = ListPoint(position, value);
		std::sort(m_points.begin(), m_points.end(), [](const ListPoint& a, const ListPoint& b)
			{
				return a.position < b.position;
			});
	}

	uint64_t InterpolationList::findPositionIdx(float position)
	{
		uint64_t index = INVALID_UINT64;
		for (uint64_t i = 0; i < m_points.size() - 1; i++)
		{
			if (m_points[i].position <= position && m_points[i + 1].position >= position)
			{
				index = i;
				break;
			}
		}

		OKAY_ASSERT(index != INVALID_UINT64);
		return index;
	}
}
