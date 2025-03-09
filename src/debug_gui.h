#ifndef DEBUG_GUI_H
#define DEBUG_GUI_H

#include <glad/glad.h> 
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <queue>
#include <vector>
#include <functional>

#include <windows.h>
#include <atlstr.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "config.h"
#include "scene.h"
#include "camera.h"

class DebugGUIWindow {
	bool _is_gui_focused = false;
	bool _do_next_gui_focus = false;
	float _last_sun_strength = 0.f;

	// fps
	float _frame_times_sum = 0.0f;
	std::queue<float> _last_frame_times;

	Camera* _camera;
	Scene* _scene;

	std::vector<const char*> _brick_names;

	std::function<void(std::string scene_name, GLFWwindow* window)> _load_scene_callback;

public:
	// gui variables
	int selected_output = 0;
	float gamma = config::Gamma;
	int blur_size = config::BlurSize;
	int current_brick = 0;
	float sun_strength =  0.f;

	void Draw(GLFWwindow* window, float delta_time, int frame_count) {
		_last_sun_strength = sun_strength;

		ImGui::SetNextWindowPos({ 0, 0 });

		ImGui::Begin("Debug Window", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

		if (_do_next_gui_focus) {
			ImGui::SetWindowFocus();
			_do_next_gui_focus = false;
		}

		if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_::ImGuiFocusedFlags_RootAndChildWindows) && _is_gui_focused) {
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			_is_gui_focused = false;
		}

		if (ImGui::Button("Load Scene")) {
			tryLoadScene(window);
		}

		ImGui::Separator();

		_last_frame_times.push(delta_time);
		_frame_times_sum += delta_time;

		if (frame_count > config::FPSAverageAmount) {
			_frame_times_sum -= _last_frame_times.front();
			_last_frame_times.pop();

			float average = config::FPSAverageAmount / _frame_times_sum;
			ImGui::Text("FPS = %d", int(config::FPSAverageAmount / _frame_times_sum));
		}
		else {
			ImGui::Text("FPS = ...");
		}

		ImGui::Text("Position: %.2f, %.2f, %.2f", _camera->position.x, _camera->position.y, _camera->position.z);

		ImGui::Checkbox("No Clip Fly", &_camera->no_clip);

		ImGui::Combo("Current Brick", &current_brick, _brick_names.data(), _brick_names.size());

		if (ImGui::CollapsingHeader("Visuals")) {
			ImGui::SliderFloat("Sun Strength", &sun_strength, 0.0, 10.0);
			ImGui::SliderFloat("Gamma", &gamma, 1.0f, 5.0f);
			ImGui::SliderInt("Blur Radius", &blur_size, 0, 10);
			ImGui::Combo("Output", &selected_output, config::OutputNames, IM_ARRAYSIZE(config::OutputNames));
		}

		ImGui::End();
	}

	void SetScene(Scene* scene, Camera* camera) {
		_scene = scene;
		_camera = camera;

		_brick_names.clear();
		current_brick = 0;

		for (int i = 0; i < scene->bricks.size(); i++) {
			_brick_names.push_back(scene->bricks[i]->name.c_str());
		}

		_frame_times_sum = 0.f;
		while (!_last_frame_times.empty()) _last_frame_times.pop();
	}

	void SetLoadSceneCallback(std::function<void(std::string scene_name, GLFWwindow* window)> load_scene_callback) {
		_load_scene_callback = load_scene_callback;
	}

	void DoFocus(GLFWwindow* window) {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		_do_next_gui_focus = true;
		_is_gui_focused = true;
	}

	bool IsFocused() {
		return _is_gui_focused;
	}

	bool CriticalVariablesChanged() {
		return _last_sun_strength != sun_strength;
	}

private:
	void tryLoadScene(GLFWwindow* window) {
		OPENFILENAME ofn;
		WCHAR file_name[100];

		// open a file name
		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = NULL;
		ofn.lpstrFile = file_name;
		ofn.lpstrFile[0] = '\0';
		ofn.nMaxFile = sizeof(file_name);
		ofn.nFilterIndex = 1;
		ofn.lpstrFileTitle = NULL;
		ofn.nMaxFileTitle = 0;
		ofn.lpstrInitialDir = NULL;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

		if (GetOpenFileName(&ofn)) {
			_load_scene_callback((std::string)CW2A(file_name), window);
		}
	}
};

#endif