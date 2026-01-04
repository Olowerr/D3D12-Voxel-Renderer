#include "App.h"
#include "Engine/World/WorldGenSettings.h"

#include "glm/common.hpp"
#include "imgui/imgui.h"
#include "imgui/implot.h"

#include "glm/gtc/type_ptr.hpp"

using namespace Okay;

App::App()
	:Application("D3D12 Voxel Renderer", 1600, 900)
{
	WorldGenerationData& worldGenData = WorldGenerationData::get();
	worldGenData.terrrainNoiseInterpolation.addPoint(-0.45f, -0.55f);
	worldGenData.terrrainNoiseInterpolation.addPoint(-0.1f, 0.f);
	worldGenData.terrrainNoiseInterpolation.addPoint(0.f, 0.1f);
	worldGenData.terrrainNoiseInterpolation.addPoint(0.275f, 0.15f);
	worldGenData.terrrainNoiseInterpolation.addPoint(0.65f, 0.525f);

	worldGenData.terrainNoiseData.numOctaves = 4;
	worldGenData.terrainNoiseData.frequencyNumerator = 1.f;
	worldGenData.terrainNoiseData.frequencyDenominator = 150.f;

	worldGenData.treeNoiseData.numOctaves = 2;
	worldGenData.treeNoiseData.frequencyNumerator = 0.835f;
	worldGenData.treeNoiseData.frequencyDenominator = 1.f;
	worldGenData.treeNoiseData.exponent = 3.39f;

	worldGenData.treeAreaNoiseData.frequencyDenominator = 100.f;
	worldGenData.treeAreaNoiseThreshold = 0.46f;
	worldGenData.treeMaxSpawnAltitude = 83;


	CloudGenerationData& cloudGenData = CloudGenerationData::get();
	cloudGenData.cloudNoise.numOctaves = 1;
	cloudGenData.cloudNoise.frequencyNumerator = 1.f;
	cloudGenData.cloudNoise.frequencyDenominator = 67.f;
	cloudGenData.cloudNoise.persistence = 0.5f;
	cloudGenData.cloudNoise.cutOff = 0.f;
	cloudGenData.cloudNoise.exponent = 1.35f;

	cloudGenData.maskNoise.numOctaves = 5;
	cloudGenData.maskNoise.frequencyNumerator = 1.f;
	cloudGenData.maskNoise.frequencyDenominator = 147.f;
	cloudGenData.maskNoise.persistence = 0.48f;
	cloudGenData.maskNoise.cutOff = 0.64f;
	cloudGenData.maskNoise.exponent = 1.f;

	m_chunkGenerator.applySeed(worldGenData.seed);
}

void App::onUpdate(TimeStep dt)
{
	updateCamera(dt);
	handleImgui(dt);
}

void App::updateCamera(TimeStep dt)
{
	if (Input::isKeyPressed(Key::E))
	{
		MouseMode newMode = Input::getMouseMode() == MouseMode::LOCKED ? MouseMode::FREE : MouseMode::LOCKED;
		Input::setMouseMode(newMode);
	}

	if (Input::getMouseMode() == MouseMode::FREE)
		return;

	float camMoveSpeed = Input::isKeyDown(Key::L_SHIFT) ? 250.f : 16.f;
	float camRotSpeed = 0.1f;

	// Movement
	float forwardDir = (float)Input::isKeyDown(Key::W) - (float)Input::isKeyDown(Key::S);
	float rightDir = (float)Input::isKeyDown(Key::D) - (float)Input::isKeyDown(Key::A);
	float upDir = (float)Input::isKeyDown(Key::SPACE) - (float)Input::isKeyDown(Key::L_CTRL);

	glm::vec3 moveDir = glm::vec3(0.f);

	if (forwardDir)
	{
		moveDir += m_camera.transform.forwardVec() * forwardDir;
	}
	if (rightDir)
	{
		moveDir += m_camera.transform.rightVec() * rightDir;
	}
	if (upDir)
	{
		moveDir += glm::vec3(0.f, 1.f, 0.f) * upDir;
	}

	if (forwardDir || rightDir || upDir)
	{
		m_camera.transform.position += glm::normalize(moveDir) * camMoveSpeed * dt;
	}


	// Rotation
	glm::vec2 mouseDelta = Input::getMouseDelta();
	m_camera.transform.rotation.y += mouseDelta.x * camRotSpeed;
	m_camera.transform.rotation.x += mouseDelta.y * camRotSpeed;


	// Zoom
	m_camera.fov = glm::clamp(m_camera.fov - Input::getScrollDelta(), 5.f, 90.f);
}

static bool imguiNoiseSamplingControls(Noise::SamplingData& samplingData, std::string_view imGuiID)
{
	ImGui::PushID(imGuiID.data());

	bool changed = false;
	changed |= ImGui::DragInt("Num Octaves", (int*)&samplingData.numOctaves, 0.2f, 1, INT_MAX);
	changed |= ImGui::DragFloat("Frequency Numerator", &samplingData.frequencyNumerator, 0.01f);
	changed |= ImGui::DragFloat("Frequency Denominator", &samplingData.frequencyDenominator, 0.1f);
	changed |= ImGui::DragFloat("Persistance", &samplingData.persistence, 0.01f);
	changed |= ImGui::DragFloat("Exponent", &samplingData.exponent, 0.01f, 0.5f, FLT_MAX);
	changed |= ImGui::DragFloat("Cutoff", &samplingData.cutOff, 0.01f);

	ImGui::PopID();

	if (samplingData.frequencyDenominator == 0.f)
		samplingData.frequencyDenominator = 0.000001f;
	
	return changed;
}

void App::handleImgui(TimeStep dt)
{
	static TimeStep passedTime = 0;
	static uint32_t numFrames = 0;
	static TimeStep avgDT = 1.f;
	passedTime += dt;
	numFrames++;

	if (passedTime > 1.f)
	{
		avgDT = passedTime / (float)numFrames;
		passedTime -= 1.f;
		numFrames = 0;
	}

	if (ImGui::Begin("Core"))
	{
		ImGui::PushItemWidth(150.f);
		glm::vec3 camPos = m_camera.transform.position;
		glm::vec3 camRot = m_camera.transform.rotation;

		ChunkID camChunkID = blockCoordToChunkID(glm::floor(m_camera.transform.position));
		glm::ivec2 camChunkPos = chunkIDToChunkCoord(camChunkID);
		
		ImGui::Text("Avg FPS: %u (%.2f ms)", uint32_t(1.f / avgDT), avgDT * 1000.f);
		ImGui::Text("Raw FPS: %u (%.2f ms)", uint32_t(1.f / dt), dt * 1000.f);

		ImGui::Separator();

		ImGui::Text("Camera");
		ImGui::Text("Position: (%.2f, %.2f, %.2f)", camPos.x, camPos.y, camPos.z);
		ImGui::Text("Rotation: (%.2f, %.2f, %.2f)", camRot.x, camRot.y, camRot.z);
		ImGui::Text("Chunk Coord: (%d, %d)", camChunkPos.x, camChunkPos.y);
		ImGui::Text("ChunkID: %llu", camChunkID);

		ImGui::Separator();
		
		WorldGenerationData& worldGenData = WorldGenerationData::get();
		ImGui::DragInt("Render Distance", (int*)&worldGenData.renderDistance, 0.075f, 0, UINT32_MAX);
		ImGui::Checkbox("Pause chunk generation", &worldGenData.pauseGen);
	}
	ImGui::End();

	if (ImGui::Begin("Clouds"))
	{
		CloudGenerationData& cloudGenData = CloudGenerationData::get();
		bool update = false;

		ImGui::PushItemWidth(150.f);

		ImGui::DragFloat2("Velocity", glm::value_ptr(cloudGenData.velocity), 0.1f);

		ImGui::DragInt("spawnHeight", (int*)&cloudGenData.spawnHeight, 0.1f);
		ImGui::DragFloat("Scale", &cloudGenData.scale, 0.01f);
		update |= ImGui::DragFloat("Height", &cloudGenData.height, 0.01f);
		update |= ImGui::DragFloat("Max Offset", &cloudGenData.maxOffset, 0.01f);
		update |= ImGui::DragFloat("Sample Distance", &cloudGenData.sampleDistance, 0.01f, 1.f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		update |= ImGui::DragInt("Visibilty Distance (chunks)", (int*)&cloudGenData.chunkVisiblityDistance, 0.1f);
		ImGui::ColorEdit4("Colour", glm::value_ptr(cloudGenData.colour));

		ImGui::Separator();
		
		ImGui::Text("Placement");
		update |= imguiNoiseSamplingControls(cloudGenData.cloudNoise, "CloudNoise");

		ImGui::Text("Mask");
		update |= imguiNoiseSamplingControls(cloudGenData.maskNoise, "CloudMask");

		if (update)
			m_world.recreateClouds();

	}
	ImGui::End();

	if (ImGui::Begin("World Generation"))
	{
		ImGui::PushItemWidth(150.f);

		ImGui::BeginDisabled(m_chunkGenerator.areChunksLoading());
		if (ImGui::Button("Reload World"))
		{
			m_world.resetWorld();
			m_renderer.unloadChunks();
		}
		ImGui::EndDisabled();

		WorldGenerationData& worldGenData = WorldGenerationData::get();

		if (ImGui::DragInt("Seed", (int*)&worldGenData.seed, 0.2f))
			m_chunkGenerator.applySeed(worldGenData.seed);

		ImGui::Separator();

		// Terrain
		ImGui::Text("Terrain");
		imguiNoiseSamplingControls(worldGenData.terrainNoiseData, "Terrain");
		ImGui::DragInt("Ocean Height", (int*)&worldGenData.oceanHeight, 0.2f);
		ImGui::DragFloat("Amplitude", &worldGenData.amplitude, 0.1f);

		ImGui::Separator();

		// Trees
		ImGui::Text("Trees");
		imguiNoiseSamplingControls(worldGenData.treeNoiseData, "Trees");
		ImGui::DragFloat("Spawn threshold", &worldGenData.treeThreshold, 0.01f);
		ImGui::DragInt("Max spawn altitude", (int*)&worldGenData.treeMaxSpawnAltitude, 0.2f);

		ImGui::Text("Tree Area");
		imguiNoiseSamplingControls(worldGenData.treeAreaNoiseData, "TreesArea");
		ImGui::DragFloat("Area threshold", &worldGenData.treeAreaNoiseThreshold, 0.01f);


		// Terrain Noise Interpoloation
		if (ImPlot::BeginPlot("Terrain Noise Interpoloation"))
		{
			ImPlot::SetupAxesLimits(-1.2, 1.2, -1.2, 1.2);

			InterpolationList& noiseInterpolation = worldGenData.terrrainNoiseInterpolation;

			if (ImGui::Button("New Point"))
			{
				noiseInterpolation.addPoint(0.f, 0.f);
			}

			const std::vector<InterpolationList::ListPoint>& points = noiseInterpolation.getPoints();

			static std::vector<ImPlotPoint> imguiPoints;
			imguiPoints.resize(points.size());

			for (uint64_t i = 0; i < points.size(); i++)
			{
				imguiPoints[i] = ImPlotPoint((double)points[i].position, (double)points[i].value);
				if (ImPlot::DragPoint((int)i, &imguiPoints[i].x, &imguiPoints[i].y, ImVec4(1.f, 0.8f, 0.5f, 1.f)))
				{
					noiseInterpolation.updatePoint(i, (float)imguiPoints[i].x, (float)imguiPoints[i].y);
				}

				if (i > points.size() - 2)
					continue;

				ImPlot::SetNextLineStyle(ImVec4(1, 0, 0, 1), 1.f);
				ImPlot::PlotLine("##1", &imguiPoints[i].x, &imguiPoints[i].y, 2, 0, 0, sizeof(ImPlotPoint));
			}

			ImPlot::EndPlot();
		}
	}
	ImGui::End();
}
