#include "World.h"
#include "Engine/Application/Input.h"
#include "Engine/Application/Window.h"
#include "Camera.h"
#include "Engine/Utilities/Random.h"
#include "Engine/Application/Time.h"
#include "ChunkGenerator.h"
#include "Engine/World/WorldGenSettings.h"
#include "Engine/Utilities/Utilities.h"

namespace Okay
{
	void World::initialize()
	{
	}

	void World::shutdown()
	{
	}

	void World::update(const Camera& camera, const ChunkGenerator& chunkGenerator, TimeStep dt)
	{
		if (WorldGenerationData::get().pauseGen)
			return;

		clearUpdatedChunks();

		std::unique_lock lock(m_chunkMutex, std::defer_lock_t());
		for (ChunkGenID chunkGenID : chunkGenerator.getCompletedChunks())
		{
			const ChunkGenerationThread& chunkGen = chunkGenerator.getChunkGenData(chunkGenID);
			ChunkID chunkID = chunkGen.chunkID;

			if (m_loadedChunks.contains(chunkID))
				continue;

			if (!lock.owns_lock())
				lock.lock();

			m_loadedChunks[chunkID] = chunkGen.chunkData;
		}
		if (lock.owns_lock())
			lock.unlock();

		updateClouds(camera, dt);
		unloadDistantChunks(camera);
	}

	BlockType World::tryGetBlockThreaded(const glm::ivec3& blockCoord) const
	{
		ChunkID chunkID = blockCoordToChunkID(blockCoord);
		glm::ivec3 chunkBlockCoord = blockCoordToChunkBlockCoord(blockCoord);
		uint32_t chunkBlockIdx = chunkBlockCoordToChunkBlockIdx(chunkBlockCoord);

		std::shared_lock lock(m_chunkMutex);
		const Chunk* pChunk = tryGetChunk(chunkID);
		return pChunk ? pChunk->blocks[chunkBlockIdx] : BlockType::INVALID;
	}

	void World::resetWorld()
	{
		std::unique_lock lock(m_chunkMutex);
		m_loadedChunks.clear();
	}

	void World::recreateClouds()
	{
		CloudGenerationData& cloudGenData = CloudGenerationData::get();

		m_cloudList.clear();
		cloudGenData.localDrift = glm::vec3(FLT_MAX);
		cloudGenData.updateTimer = CloudGenerationData::UPDATE_INTERVAL;
	}

	void World::updateClouds(const Camera& camera, TimeStep dt)
	{
		CloudGenerationData& cloudGenData = CloudGenerationData::get();

		cloudGenData.globalDrift += cloudGenData.velocity * dt;
		cloudGenData.localDrift += cloudGenData.velocity * dt;

		cloudGenData.updateTimer += dt;
		if (cloudGenData.updateTimer < CloudGenerationData::UPDATE_INTERVAL)
			return;

		cloudGenData.updateTimer -= CloudGenerationData::UPDATE_INTERVAL;

		clearDistanceClouds(camera);
		generateCloudList(camera);

		cloudGenData.localDrift = glm::vec2(camera.transform.position.x, camera.transform.position.z);
	}

	void World::generateCloudList(const Camera& camera)
	{
		CloudGenerationData& cloudGenData = CloudGenerationData::get();

		float viewDistance = (float)cloudGenData.chunkVisiblityDistance * CHUNK_WIDTH;
		glm::vec2 cameraXZPos = glm::vec2(camera.transform.position.x, camera.transform.position.z);

		glm::vec2 min = glm::min(cloudGenData.localDrift, cameraXZPos);
		glm::vec2 max = glm::max(cloudGenData.localDrift, cameraXZPos);

		glm::vec2 overlappingMin = max - glm::vec2(viewDistance);
		glm::vec2 overlappingMax = min + glm::vec2(viewDistance);

		for (float x = -viewDistance + cameraXZPos.x; x <= viewDistance + cameraXZPos.x; x += cloudGenData.sampleDistance)
		{
			for (float z = -viewDistance + cameraXZPos.y; z <= viewDistance + cameraXZPos.y; z += cloudGenData.sampleDistance)
			{
				if (x >= overlappingMin.x && x <= overlappingMax.x && z >= overlappingMin.y && z <= overlappingMax.y)
					continue;

				sampleCloud(x - cloudGenData.globalDrift.x, z - cloudGenData.globalDrift.y);
			}
		}
	}

	void World::clearDistanceClouds(const Camera& camera)
	{
		CloudGenerationData& cloudGenData = CloudGenerationData::get();

		float viewDistance = (float)cloudGenData.chunkVisiblityDistance * CHUNK_WIDTH;
		glm::vec3 globalDriftVec3 = glm::vec3(cloudGenData.globalDrift.x, 0, cloudGenData.globalDrift.y);

		for (int32_t i = (int32_t)m_cloudList.size() - 1; i >= 0; i--)
		{
			glm::vec3 cloudGlobalPos = m_cloudList[i] + globalDriftVec3;
			glm::vec3 camToCloud = glm::abs(cloudGlobalPos - camera.transform.position);

			if (camToCloud.x > viewDistance || camToCloud.z > viewDistance)
			{
				m_cloudList.erase(m_cloudList.begin() + i);
			}
		}
	}

	void World::sampleCloud(float x, float z)
	{
		CloudGenerationData& cloudGenData = CloudGenerationData::get();

		float cloudNoise = Noise::samplePerlin2D_zeroOne(x, z, cloudGenData.cloudNoise);
		float maskNoise = Noise::samplePerlin2D_zeroOne(x, z, cloudGenData.maskNoise);
		float finalNoise = cloudNoise * maskNoise;

		float cloudHeight = finalNoise * cloudGenData.height;

		float currentHeight = 0.f;
		while (currentHeight < cloudHeight)
		{
			uint32_t seed = uint32_t(finalNoise * UINT_MAX + currentHeight);

			glm::vec3 placementOffset = glm::vec3(
				Random::randomFloat(seed) * 2.f - 1.f,
				(Random::randomFloat(seed) * 2.f - 1.f) * 0.5f,
				Random::randomFloat(seed) * 2.f - 1.f);

			placementOffset = glm::normalize(placementOffset) * cloudGenData.maxOffset * Random::randomFloat(seed);
			glm::vec3 cloudPoint = glm::vec3(x, cloudGenData.spawnHeight + currentHeight, z);

			m_cloudList.emplace_back(cloudPoint + placementOffset);
			currentHeight += cloudGenData.sampleDistance;
		}
	}

	const std::vector<glm::vec3>& World::getCloudList() const
	{
		return m_cloudList;
	}

	Chunk& World::getChunk(ChunkID chunkID)
	{
		return m_loadedChunks[chunkID];
	}

	const Chunk& World::getChunkConst(ChunkID chunkID) const
	{
		auto iterator = m_loadedChunks.find(chunkID);
		OKAY_ASSERT(iterator != m_loadedChunks.end());
		return iterator->second;
	}

	const Chunk* World::tryGetChunk(ChunkID chunkID) const
	{
		auto iterator = m_loadedChunks.find(chunkID);
		return iterator == m_loadedChunks.end() ? nullptr : &iterator->second;
	}

	bool World::isChunkLoaded(ChunkID chunkID) const
	{
		return m_loadedChunks.contains(chunkID);
	}

	void World::clearUpdatedChunks()
	{
		m_removedChunks.clear();
	}

	void World::unloadDistantChunks(const Camera& camera)
	{
		std::unique_lock lock(m_chunkMutex, std::defer_lock_t());

		glm::ivec2 camChunkCoord = vec3CoordToChunkCoord(camera.transform.position);
		auto chunkIterator = m_loadedChunks.begin();
		while (chunkIterator != m_loadedChunks.end())
		{
			ChunkID chunkID = chunkIterator->first;
			if (isChunkWithinRenderDistance(chunkID, camChunkCoord))
			{
				++chunkIterator;
				continue;
			}
		
			if (!lock.owns_lock())
				lock.lock();
		
			chunkIterator = m_loadedChunks.erase(chunkIterator);
			m_removedChunks.emplace_back(chunkID);
		}
	}

	const std::vector<ChunkID>& World::getRemovedChunks() const
	{
		return m_removedChunks;
	}
}
