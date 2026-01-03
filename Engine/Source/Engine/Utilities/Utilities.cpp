#include "Utilities.h"
#include "Engine/World/WorldGenSettings.h"

namespace Okay
{
	bool isChunkWithinRenderDistance(ChunkID chunkID, glm::ivec2 chunkCoord)
	{
		WorldGenerationData& worldGenData = WorldGenerationData::get();

		glm::vec2 chunkPos = glm::vec2(chunkIDToChunkCoord(chunkID));
		glm::vec2 camPos = glm::vec2(chunkCoord);
		return glm::length2(chunkPos - camPos) <= worldGenData.renderDistance * worldGenData.renderDistance;
	}

	bool isChunkInView(ChunkID chunkID, const Camera& camera)
	{
		glm::vec3 chunkExtents = glm::vec3(CHUNK_WIDTH, WORLD_HEIGHT, CHUNK_WIDTH) * 0.5f;
		glm::vec3 chunkCenter = chunkCoordToWorldCoord(chunkIDToChunkCoord(chunkID));
		chunkCenter += chunkExtents;

		Collision::AABB chunkBox = Collision::createAABB(chunkCenter, chunkExtents);
		return Collision::frustumAABB(camera.frustum, chunkBox);
	}
}
