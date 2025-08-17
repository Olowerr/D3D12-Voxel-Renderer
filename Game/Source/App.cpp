#include "App.h"

#include "glm/common.hpp"
#include "imgui/imgui.h"
#include "imgui/implot.h"

using namespace Okay;

App::App()
	:Application("D3D12 Voxel Renderer", 1600, 900)
{
}

void App::onUpdate(TimeStep dt)
{
	updateCamera(dt);
	handleImgui();

	static TimeStep passedTime = 0;
	static uint32_t numFrames = 0;

	passedTime += dt;
	numFrames++;

	//printf("%.4f\n", dt * 1000.f);

	if (passedTime >= 1.f)
	{
		TimeStep averageFps = passedTime / (float)numFrames;
		passedTime -= 1.f;
		numFrames = 0;

		static std::string windowTitle;
		windowTitle = "D3D12 Voxel Renderer | Fps: " + std::to_string((uint32_t)glm::round(1.f / averageFps));
		m_window.setWindowTitle(windowTitle);

		glm::vec3 camPos = m_camera.transform.position;
		glm::ivec2 chunkCoord = chunkIDToChunkCoord(blockCoordToChunkID(camPos));

		//printf("(%d, %d) | (%.1f, %.1f, %.1f)\n", chunkCoord.x, chunkCoord.y, camPos.x, camPos.y, camPos.z);
	}
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

	float camMoveSpeed = Input::isKeyDown(Key::L_SHIFT) ? 50.f : 16.f;
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

static void imguiNoiseSamplingControls(Noise::SamplingData& samplingData, std::string_view imGuiID)
{
	ImGui::PushID(imGuiID.data());

	ImGui::DragInt("Num Octaves", (int*)&samplingData.numOctaves, 0.2f);
	ImGui::DragFloat("Frequency Numerator", &samplingData.frequencyNumerator, 0.01f);
	ImGui::DragFloat("Frequency Denominator", &samplingData.frequencyDenominator, 0.1f);
	ImGui::DragFloat("Persistance", &samplingData.persistence, 0.01f);
	ImGui::DragFloat("Exponent", &samplingData.exponent, 0.01f);

	ImGui::PopID();

	if (samplingData.frequencyDenominator == 0.f)
		samplingData.frequencyDenominator = 0.000001f;
}

void App::handleImgui()
{
	if (!ImGui::Begin("World Generation"))
	{
		ImGui::End();
		return;
	}

	if (ImGui::Button("Reload World"))
	{
		m_world.resetWorld();
		m_renderer.unloadChunks();
	}

	WorldGenerationData& worldGenData = m_world.m_worldGenData;

	if (ImGui::DragInt("Seed", (int*)&worldGenData.seed, 0.2f))
		m_world.applySeed();

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
	ImGui::DragFloat("Tree threshold", &worldGenData.treeThreshold, 0.01f);


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
	
	ImGui::End();
}
