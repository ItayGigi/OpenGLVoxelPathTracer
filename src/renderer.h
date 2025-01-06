#ifndef RENDERER_H
#define RENDERER_H

#include <glad/glad.h> 
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <queue>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "config.h"
#include "drawutil.h"
#include "scene.h"
#include "mathutil.h"
#include "shader.h"
#include "camera.h"
#include "brick.h"


enum BufferTexture {
	SCREEN_TEXTURE = 0,
	HISTORY_TEXTURE,
	DEPTH_TEXTURE,
	ALBEDO_TEXTURE,
	NORMAL_TEXTURE,
	EMISSION_TEXTURE,
};

class Renderer
{
	// window
	int window_width, window_height;
	bool is_mouse_enabled = true;
	bool do_next_focus = false;

	// camera
	Camera camera;
	Camera last_camera;

	float last_mouse_x, last_mouse_y;
	bool first_mouse = true;

	Scene scene;

	// timing
	float delta_time = 0.0f;	// time between current frame and last frame
	float last_frame_time = 0.0f;
	unsigned int frame_count = 0;

	// fps
	float frame_times_sum = 0.0f;
	std::queue<float> last_frame_times;

	unsigned int VAO;

	// frame buffers
	unsigned int fbo1, fbo2;
	unsigned int buffer_textures1[6], buffer_textures2[6];

	unsigned int scene_tex, bricks_tex, mats_tex;

	int selected_output = 0;
	float gamma = config::Gamma;
	int blur_size = config::BlurSize;

	glm::ivec3 selected_brick;
	glm::ivec3 selected_brick_normal;

	std::unique_ptr<Shader> shader;
	std::unique_ptr<Shader> post_process_shader;

public:
	int Initialize(GLFWwindow* window, int argc, const char* argv[]) {
		glfwSwapInterval(config::VSYNC);

		shader = std::make_unique<Shader>(Shader(config::VertexShaderPath, config::FragShaderPath));
		post_process_shader = std::make_unique<Shader>(Shader(config::VertexShaderPath, config::PostFragShaderPath));

		drawUtils::setLineWidth(config::LineWidth);

		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
		//io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch

		// Setup Platform/Renderer backends
		ImGui_ImplGlfw_InitForOpenGL(window, true);          // Second param install_callback=true will install GLFW callbacks and chain to existing ones.
		ImGui_ImplOpenGL3_Init();

		VAO = createVAO();

		drawUtils::initLineShader();

		// load scene
		if (argc < 2) {
			std::cerr << "Scene name expected as an argument. Exiting." << std::endl;
			return 2;
		}

		if (!loadScene(argv[1], &scene_tex, &bricks_tex, &mats_tex)) {
			std::cerr << "Failed to load scene. Exiting." << std::endl;
			glDeleteTextures(1, &scene_tex);
			glDeleteTextures(1, &bricks_tex);
			glDeleteTextures(1, &mats_tex);
			glfwTerminate();
			return 1;
		}

		glGenFramebuffers(1, &fbo1);
		glGenFramebuffers(1, &fbo2);

		return 0;
	}

	void Update(GLFWwindow* window) {
		frame_count++;
		float current_frame_time = static_cast<float>(glfwGetTime());
		delta_time = current_frame_time - last_frame_time;
		last_frame_time = current_frame_time;

		camera.Update(delta_time, &scene);

		glfwPollEvents();

		processInput(window);

		// raycast selected brick
		util::RayHit hit = scene.CastRay(camera.position, camera.front, config::MaxHighlightDistance);

		if (hit.hit) selected_brick = camera.position + (hit.dist + 0.0001f) * camera.front;
		else selected_brick = glm::ivec3(-1);

		selected_brick_normal = hit.normal;

		draw();

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		createDebugImGuiWindow(window);

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// swap buffers
		std::swap(fbo1, fbo2);
		std::swap(buffer_textures1, buffer_textures2);

		last_camera = camera;
	}

	void Terminate() {
		// delete all textures
		glDeleteTextures(1, &scene_tex);
		glDeleteTextures(1, &bricks_tex);
		glDeleteTextures(1, &mats_tex);
		for (int i = 0; i < sizeof(buffer_textures1) / sizeof(unsigned int); i++) {
			glDeleteTextures(1, &buffer_textures1[i]);
			glDeleteTextures(1, &buffer_textures2[i]);
		}

		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	void HandleFramebufferSizeCallback(GLFWwindow* window, int width, int height)
	{
		glViewport(0, 0, width, height);
		window_width = width;
		window_height = height;

		for (int i = 0; i < sizeof(buffer_textures1) / sizeof(unsigned int); i++) {
			glDeleteTextures(1, &buffer_textures1[i]);
			glDeleteTextures(1, &buffer_textures2[i]);
		}

		unsigned int attachments[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5 };

		for (int i = 0; i < 2; i++)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, fbo1);
			// generate textures
			for (int i = 0; i < sizeof(buffer_textures1) / sizeof(unsigned int); i++) {
				glGenTextures(1, &buffer_textures1[i]);
				glActiveTexture(GL_TEXTURE0 + 5 + i);
				glBindTexture(GL_TEXTURE_2D, buffer_textures1[i]);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

				glFramebufferTexture2D(GL_FRAMEBUFFER, attachments[i], GL_TEXTURE_2D, buffer_textures1[i], 0);

				glBindTexture(GL_TEXTURE_2D, 0);
			}

			glActiveTexture(GL_TEXTURE0 + 5 + SCREEN_TEXTURE);
			glBindTexture(GL_TEXTURE_2D, buffer_textures1[SCREEN_TEXTURE]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, window_width, window_height, 0, GL_RGB, GL_FLOAT, NULL);

			glActiveTexture(GL_TEXTURE0 + 5 + HISTORY_TEXTURE);
			glBindTexture(GL_TEXTURE_2D, buffer_textures1[HISTORY_TEXTURE]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, window_width, window_height, 0, GL_RED, GL_FLOAT, NULL);

			glActiveTexture(GL_TEXTURE0 + 5 + DEPTH_TEXTURE);
			glBindTexture(GL_TEXTURE_2D, buffer_textures1[DEPTH_TEXTURE]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, window_width, window_height, 0, GL_RED, GL_FLOAT, NULL);

			glActiveTexture(GL_TEXTURE0 + 5 + ALBEDO_TEXTURE);
			glBindTexture(GL_TEXTURE_2D, buffer_textures1[ALBEDO_TEXTURE]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, window_width, window_height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

			glActiveTexture(GL_TEXTURE0 + 5 + NORMAL_TEXTURE);
			glBindTexture(GL_TEXTURE_2D, buffer_textures1[NORMAL_TEXTURE]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8I, window_width, window_height, 0, GL_RGB_INTEGER, GL_INT, NULL);

			glActiveTexture(GL_TEXTURE0 + 5 + EMISSION_TEXTURE);
			glBindTexture(GL_TEXTURE_2D, buffer_textures1[EMISSION_TEXTURE]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, window_width, window_height, 0, GL_RED, GL_FLOAT, NULL);

			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;

			glDrawBuffers(sizeof(attachments) / sizeof(unsigned int), attachments);

			std::swap(fbo1, fbo2);
			std::swap(buffer_textures1, buffer_textures2);
		}

		glBindBuffer(GL_FRAMEBUFFER, 0);

		drawUtils::passResolution(window_width, window_height);
	}

	// glfw: whenever the mouse moves, this callback is called
	void HandleMouseCallback(GLFWwindow* window, double x_pos_in, double y_pos_in)
	{
		if (is_mouse_enabled) {
			first_mouse = true;
			return;
		}

		float xpos = static_cast<float>(x_pos_in);
		float ypos = static_cast<float>(y_pos_in);

		if (first_mouse)
		{
			last_mouse_x = xpos;
			last_mouse_y = ypos;
			first_mouse = false;
		}

		float xoffset = xpos - last_mouse_x;
		float yoffset = last_mouse_y - ypos; // reversed since y-coordinates go from bottom to top

		last_mouse_x = xpos;
		last_mouse_y = ypos;

		camera.ProcessMouseMovement(xoffset, yoffset);
	}

	// glfw: whenever the mouse scroll wheel scrolls, this callback is called
	void HandleScrollCallback(GLFWwindow* window, double x_offset, double y_offset)
	{
		camera.ProcessMouseScroll(static_cast<float>(y_offset));
	}

	void HandleMouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
		if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && !is_mouse_enabled && selected_brick != glm::ivec3(-1)) {
			scene.brick_map->setVoxel(selected_brick.x, selected_brick.y, selected_brick.z, 0); // delete selected brick

			// update texture
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, scene_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, scene.brick_map->size.x * scene.brick_map->size.y / 8, scene.brick_map->size.z, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, scene.brick_map->data.data());
		}

		if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS && !is_mouse_enabled && selected_brick != glm::ivec3(-1)) {
			scene.brick_map->setVoxel(selected_brick.x + selected_brick_normal.x, selected_brick.y + selected_brick_normal.y, selected_brick.z + selected_brick_normal.z, 1); // place brick

			// update texture
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, scene_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, scene.brick_map->size.x * scene.brick_map->size.y / 8, scene.brick_map->size.z, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, scene.brick_map->data.data());
		}
	}

private:
	// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
	void processInput(GLFWwindow* window)
	{
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(window, true);

		if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS) {
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			is_mouse_enabled = true;
			do_next_focus = true;
		}

		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
			camera.ProcessKeyboard(FORWARD, delta_time, &scene);
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
			camera.ProcessKeyboard(BACKWARD, delta_time, &scene);
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
			camera.ProcessKeyboard(LEFT, delta_time, &scene);
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
			camera.ProcessKeyboard(RIGHT, delta_time, &scene);
		if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
			camera.ProcessKeyboard(UP, delta_time, &scene);
		if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
			camera.ProcessKeyboard(DOWN, delta_time, &scene);
	}

	void createDebugImGuiWindow(GLFWwindow* window) {
		ImGui::SetNextWindowPos({ 0, 0 });

		ImGui::Begin("Debug Window", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

		if (do_next_focus) {
			ImGui::SetWindowFocus();
			do_next_focus = false;
		}

		if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_::ImGuiFocusedFlags_RootAndChildWindows) && is_mouse_enabled) {
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			is_mouse_enabled = false;
		}

		last_frame_times.push(delta_time);
		frame_times_sum += delta_time;

		if (frame_count > config::FPSAverageAmount) {
			frame_times_sum -= last_frame_times.front();
			last_frame_times.pop();

			float average = config::FPSAverageAmount / frame_times_sum;
			ImGui::Text("FPS = %d", int(config::FPSAverageAmount / frame_times_sum));
		}
		else {
			ImGui::Text("FPS = ...");
		}

		ImGui::Text("Position: %.2f, %.2f, %.2f", camera.position.x, camera.position.y, camera.position.z);

		ImGui::Checkbox("No Clip Fly", &camera.no_clip);

		if (ImGui::CollapsingHeader("Visuals")) {
			ImGui::SliderFloat("Gamma", &gamma, 1.0f, 5.0f);
			ImGui::SliderInt("Blur Radius", &blur_size, 0, 10);
			ImGui::Combo("Output", &selected_output, config::OutputNames, IM_ARRAYSIZE(config::OutputNames));
		}

		ImGui::End();
	}

	unsigned int createVAO() {
		// create and load vertices
		float vertices[] = {
			 1.0f,  1.0f, 0.0f,		1.0f, 1.0f,    // top right
			 1.0f, -1.0f, 0.0f,		1.0f, -1.0f,   // bottom right
			-1.0f, -1.0f, 0.0f,		-1.0f, -1.0f,  // bottom left
			-1.0f,  1.0f, 0.0f,		-1.0f, 1.0f    // top left 
		};

		unsigned int indices[] = {
			0, 1, 3,  // first Triangle
			1, 2, 3   // second Triangle
		};

		unsigned int VBO, VAO, EBO;
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &EBO);
		// bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
		glBindVertexArray(VAO);

		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);

		// note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		return VAO;
	}

	void draw() {
		glBindFramebuffer(GL_FRAMEBUFFER, fbo1);
		glClear(GL_COLOR_BUFFER_BIT);

		shader->use();

		shader->setMat4("CamRotation", glm::mat4_cast(camera.GetRotation()));
		shader->setVec3("CamPosition", camera.position);

		shader->setMat4("LastCamRotation", glm::mat4_cast(last_camera.GetRotation()));
		shader->setVec3("LastCamPosition", last_camera.position);

		shader->setUVec2("Resolution", window_width, window_height);

		shader->setUInt("FrameCount", frame_count);

		shader->setTexture("LastFrameTex", buffer_textures2[SCREEN_TEXTURE], 5 + SCREEN_TEXTURE);
		shader->setTexture("HistoryTex", buffer_textures2[HISTORY_TEXTURE], 5 + HISTORY_TEXTURE);
		shader->setTexture("LastDepthTex", buffer_textures2[DEPTH_TEXTURE], 5 + DEPTH_TEXTURE);
		shader->setTexture("LastNormalTex", buffer_textures2[NORMAL_TEXTURE], 5 + NORMAL_TEXTURE);

		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

		// post processing
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		post_process_shader->use();

		post_process_shader->setUVec2("Resolution", window_width, window_height);
		post_process_shader->setInt("OutputNum", selected_output);
		post_process_shader->setFloat("Gamma", gamma);
		post_process_shader->setInt("BlurSize", blur_size);

		post_process_shader->setTexture("Texture", buffer_textures1[SCREEN_TEXTURE], 5 + SCREEN_TEXTURE);
		post_process_shader->setTexture("AlbedoTex", buffer_textures1[ALBEDO_TEXTURE], 5 + ALBEDO_TEXTURE);
		post_process_shader->setTexture("EmissionTex", buffer_textures1[EMISSION_TEXTURE], 5 + EMISSION_TEXTURE);
		post_process_shader->setTexture("NormalTex", buffer_textures1[NORMAL_TEXTURE], 5 + NORMAL_TEXTURE);
		post_process_shader->setTexture("DepthTex", buffer_textures1[DEPTH_TEXTURE], 5 + DEPTH_TEXTURE);
		post_process_shader->setTexture("HistoryTex", buffer_textures1[HISTORY_TEXTURE], 5 + HISTORY_TEXTURE);

		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

		// selected highlight outline
		drawUtils::passDepthTexture(buffer_textures1[DEPTH_TEXTURE], 5 + DEPTH_TEXTURE);

		drawUtils::line_color = config::SelectedLineColor;
		drawSelectedBrickLines();

		// crosshair
		drawUtils::line_color = config::CrosshairColor;
		drawUtils::drawLine(glm::vec2(-config::CrosshairSize * window_height / window_width, 0.), glm::vec2(config::CrosshairSize * window_height / window_width, 0.));
		drawUtils::drawLine(glm::vec2(0., -config::CrosshairSize), glm::vec2(0., config::CrosshairSize));

		drawUtils::drawLinesFlush();
	}

	bool loadScene(const std::string scene_path, unsigned int* scene_texture, unsigned int* bricks_texture, unsigned int* mats_texture) {
		if (!scene.LoadFromFile(scene_path)) return false;

		shader->use();
		shader->setUVec3("MapSize", scene.brick_map->size.x, scene.brick_map->size.y, scene.brick_map->size.z);

		glGenTextures(1, scene_texture);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, *scene_texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, scene.brick_map->size.x * scene.brick_map->size.y / 8, scene.brick_map->size.z, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, scene.brick_map->data.data());
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glUniform1i(glGetUniformLocation(shader->ID, "BrickMap"), 0);

		// bricks
		glGenTextures(1, bricks_texture);
		glActiveTexture(GL_TEXTURE0 + 1);
		glBindTexture(GL_TEXTURE_2D_ARRAY, *bricks_texture);

		// allocate bricks texture array
		glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_R32UI, config::BrickSize * config::BrickSize / 8, config::BrickSize, scene.bricks.size(), 0, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);

		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		for (int i = 0; i < scene.bricks.size(); i++)
		{
			// assign brick data
			glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, config::BrickSize * config::BrickSize / 8, config::BrickSize, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, scene.bricks[i]->data.data());
		}

		glUniform1i(glGetUniformLocation(shader->ID, "BricksTex"), 1);

		// materials
		glGenTextures(1, mats_texture);
		glActiveTexture(GL_TEXTURE0 + 2);
		glBindTexture(GL_TEXTURE_2D, *mats_texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32UI, 16, scene.bricks.size(), 0, GL_RG_INTEGER, GL_UNSIGNED_INT, scene.mats_data.data());
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

		glUniform1i(glGetUniformLocation(shader->ID, "MatsTex"), 2);

		shader->setVec3("EnvironmentColor", scene.brick_map->env_color);

		camera = Camera(scene.brick_map->camera_start_pos, { {0.0f},{1.0f},{0.0f} }, scene.brick_map->camera_start_angles.y, scene.brick_map->camera_start_angles.x);;

		return true;
	}

	void drawSelectedBrickLines() {
		if (selected_brick == glm::ivec3(-1)) return;

		glm::vec3 p1, p2;

		for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) {
			p1 = selected_brick + glm::ivec3(0, i, j);
			p2 = selected_brick + glm::ivec3(1, i, j);
			drawUtils::drawLineDepth(
				camera.WorldToScreen(p1, window_width, window_height), camera.WorldToView(p1),
				camera.WorldToScreen(p2, window_width, window_height), camera.WorldToView(p2));

			p1 = selected_brick + glm::ivec3(i, 0, j);
			p2 = selected_brick + glm::ivec3(i, 1, j);
			drawUtils::drawLineDepth(
				camera.WorldToScreen(p1, window_width, window_height), camera.WorldToView(p1),
				camera.WorldToScreen(p2, window_width, window_height), camera.WorldToView(p2));

			p1 = selected_brick + glm::ivec3(i, j, 0);
			p2 = selected_brick + glm::ivec3(i, j, 1);
			drawUtils::drawLineDepth(
				camera.WorldToScreen(p1, window_width, window_height), camera.WorldToView(p1),
				camera.WorldToScreen(p2, window_width, window_height), camera.WorldToView(p2));
		}
	}

};

#endif