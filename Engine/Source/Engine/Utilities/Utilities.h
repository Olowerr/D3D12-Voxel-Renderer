#pragma once
#include "Engine/World/Chunk.h"
#include "Engine/World/Camera.h"

namespace Okay
{
	bool isChunkInView(ChunkID chunkID, const Camera& camera);

	bool isChunkWithinRenderDistance(ChunkID chunkID, glm::ivec2 chunkCoord);
}
