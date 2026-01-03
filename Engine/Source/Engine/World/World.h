#pragma once

#include "Chunk.h"
#include "Engine/Utilities/InterpolationList.h"
#include "Engine/Utilities/Noise.h"
#include "Engine/Utilities/ThreadPool.h"
#include "Engine/Application/Time.h"
#include "Structure.h"

#include <atomic>
#include <unordered_map>
#include <shared_mutex>

namespace Okay
{
	class Window;
	class ChunkGenerator;
	struct Camera;

	class World
	{
	public:
		World() = default;
		~World() = default;

		void initialize();
		void shutdown();

		void update(const Camera& camera, const ChunkGenerator& chunkGenerator, TimeStep dt);

		BlockType tryGetBlockThreaded(const glm::ivec3& blockCoord) const;

		Chunk& getChunk(ChunkID chunkID);
		const Chunk& getChunkConst(ChunkID chunkID) const;
		const Chunk* tryGetChunk(ChunkID chunkID) const;
		bool isChunkLoaded(ChunkID chunkID) const;

		const std::vector<ChunkID>& getRemovedChunks() const;

		void resetWorld();

		void recreateClouds();
		const std::vector<glm::vec3>& getCloudList() const;

	private:
		void clearUpdatedChunks();
		void unloadDistantChunks(const Camera& camera);

		void updateClouds(const Camera& camera, TimeStep dt);
		void generateCloudList(const Camera& camera);
		void clearDistanceClouds(const Camera& camera);
		void sampleCloud(float x, float z);

	private:
		mutable std::shared_mutex m_chunkMutex;
		std::unordered_map<ChunkID, Chunk> m_loadedChunks;

		std::vector<ChunkID> m_removedChunks;
		std::vector<glm::vec3> m_cloudList;

	};
}
