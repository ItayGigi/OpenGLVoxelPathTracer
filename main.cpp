#include <glad/glad.h> 
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <fstream>
#include <queue>
#include "shader.h"
#include "camera.h"
#include "brick.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

enum BufferTexture {
	SCREEN_TEXTURE = 0,
	HISTORY_TEXTURE,
	DEPTH_TEXTURE,
	ALBEDO_TEXTURE,
	NORMAL_TEXTURE,
	EMISSION_TEXTURE,
};

void framebufferSizeCallback(GLFWwindow* window, int width, int height);
void mouseCallback(GLFWwindow* window, double x_pos, double y_pos);
void scrollCallback(GLFWwindow* window, double x_offset, double y_offset);
void processInput(GLFWwindow* window);
void createDebugImGuiWindow();
unsigned int createVAO();
void draw(Shader shader, Shader postShader, unsigned int VAO);
bool loadScene(Shader shader, const std::string scene_path, unsigned int* map_texture, unsigned int* bricks_texture, unsigned int* mats_texture);
bool isPositionOccupied(const glm::vec3 pos);


// constants
const std::string	kScenePath = "menger.scene";
const std::string	kAssetsFolder = "assets/";
const bool			kVSYNC = false;
const char*			kOutputNames[] = { "Result", "Composite", "Illumination", "Albedo", "Emission", "Normal", "Depth", "History"};
const unsigned int	kFPSAverageAmount = 150;


// window
GLFWwindow* window;
int window_width = 1000, window_height = 700;
bool is_mouse_enabled = true;
bool do_next_focus = false;

// camera
Camera camera;
Camera last_camera;

float last_mouse_x = window_width / 2.0f;
float last_mouse_y = window_height / 2.0f;
bool first_mouse = true;

std::unique_ptr<BrickMap> brick_map;
std::vector<std::unique_ptr<Brick>> bricks;

// timing
float delta_time = 0.0f;	// time between current frame and last frame
float last_frame_time = 0.0f;
unsigned int frame_count = 0;

// fps
float frame_times_sum = 0.0f;
std::queue<float> last_frame_times;

// frame buffers
unsigned int fbo1, fbo2;
unsigned int buffer_textures1[6], buffer_textures2[6];

int selected_output = 0;
float gamma = 2.2f;

int main() {
	// initialize glfw
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// create window
	window = glfwCreateWindow(window_width, window_height, "My Window", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
	glfwSetCursorPosCallback(window, mouseCallback);
	glfwSetScrollCallback(window, scrollCallback);
	glfwSwapInterval(kVSYNC);

	// initialize glad
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		glfwTerminate();
		return -1;
	}

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

	unsigned int VAO = createVAO();

	Shader shader("vertex.vert", "fragment.frag");
	Shader postProcessShader("vertex.vert", "postprocessing.frag");

	// load scene
	unsigned int sceneTex, bricksTex, matsTex;
	if (!loadScene(shader, kScenePath, &sceneTex, &bricksTex, &matsTex)) {
		std::cerr << "failed to load scene. exiting" << std::endl;
		glDeleteTextures(1, &sceneTex);
		glDeleteTextures(1, &bricksTex);
		glDeleteTextures(1, &matsTex);
		glfwTerminate();
		return -1;
	}

	glGenFramebuffers(1, &fbo1);
	glGenFramebuffers(1, &fbo2);

	framebufferSizeCallback(window, window_width, window_height);

	// render loop
	while (!glfwWindowShouldClose(window))
	{
		frame_count++;
		float currentFrameTime = static_cast<float>(glfwGetTime());
		delta_time = currentFrameTime - last_frame_time;
		last_frame_time = currentFrameTime;

		camera.Update(delta_time, &isPositionOccupied, brick_map->size * 8);

		processInput(window);

		glfwPollEvents();

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		createDebugImGuiWindow();

		draw(shader, postProcessShader, VAO);

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
	}

	glDeleteTextures(1, &sceneTex);
	glDeleteTextures(1, &bricksTex);
	glDeleteTextures(1, &matsTex);
	for (int i = 0; i < sizeof(buffer_textures1) / sizeof(unsigned int); i++) {
		glDeleteTextures(1, &buffer_textures1[i]);
		glDeleteTextures(1, &buffer_textures2[i]);
	}

	glfwTerminate();
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	return 0;
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height)
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
}

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

	glm::ivec3 map_size = brick_map->size * 8;

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		camera.ProcessKeyboard(FORWARD, delta_time, &isPositionOccupied, map_size);
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		camera.ProcessKeyboard(BACKWARD, delta_time, &isPositionOccupied, map_size);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		camera.ProcessKeyboard(LEFT, delta_time, &isPositionOccupied, map_size);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		camera.ProcessKeyboard(RIGHT, delta_time, &isPositionOccupied, map_size);
	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
		camera.ProcessKeyboard(UP, delta_time, &isPositionOccupied, map_size);
	if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
		camera.ProcessKeyboard(DOWN, delta_time, &isPositionOccupied, map_size);
}

// glfw: whenever the mouse moves, this callback is called
void mouseCallback(GLFWwindow* window, double x_pos_in, double y_pos_in)
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
void scrollCallback(GLFWwindow* window, double x_offset, double y_offset)
{
	camera.ProcessMouseScroll(static_cast<float>(y_offset));
}

void createDebugImGuiWindow() {
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

	if (frame_count > kFPSAverageAmount) {
		frame_times_sum -= last_frame_times.front();
		last_frame_times.pop();

		float average = kFPSAverageAmount / frame_times_sum;
		ImGui::Text("FPS = %d", int(kFPSAverageAmount / frame_times_sum));
	}
	else {
		ImGui::Text("FPS = ...");
	}

	ImGui::Text("Position: %.2f, %.2f, %.2f", camera.position.x, camera.position.y, camera.position.z);

	ImGui::Checkbox("No Clip Fly", &camera.no_clip);

	if (ImGui::CollapsingHeader("Visuals")) {
		ImGui::SliderFloat("Gamma", &gamma, 1.0f, 5.0f);
		ImGui::Combo("Output", &selected_output, kOutputNames, IM_ARRAYSIZE(kOutputNames));
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

void draw(Shader shader, Shader post_shader, unsigned int vao) {
	glBindFramebuffer(GL_FRAMEBUFFER, fbo1);
	glClear(GL_COLOR_BUFFER_BIT);

	shader.use();

	shader.setMat4("CamRotation", glm::mat4_cast(camera.GetRotation()));
	shader.setVec3("CamPosition", camera.position);

	shader.setMat4("LastCamRotation", glm::mat4_cast(last_camera.GetRotation()));
	shader.setVec3("LastCamPosition", last_camera.position);

	shader.setUVec2("Resolution", window_width, window_height);

	shader.setUInt("FrameCount", frame_count);

	shader.setTexture("LastFrameTex", buffer_textures2[SCREEN_TEXTURE], 5 + SCREEN_TEXTURE);
	shader.setTexture("HistoryTex", buffer_textures2[HISTORY_TEXTURE], 5 + HISTORY_TEXTURE);
	shader.setTexture("LastDepthTex", buffer_textures2[DEPTH_TEXTURE], 5 + DEPTH_TEXTURE);
	shader.setTexture("LastNormalTex", buffer_textures2[NORMAL_TEXTURE], 5 + NORMAL_TEXTURE);

	glBindVertexArray(vao);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	post_shader.use();
	
	post_shader.setUVec2("Resolution", window_width, window_height);
	post_shader.setInt("OutputNum", selected_output);
	post_shader.setFloat("Gamma", gamma);
	
	post_shader.setTexture("Texture", buffer_textures1[SCREEN_TEXTURE], 5 + SCREEN_TEXTURE);
	post_shader.setTexture("AlbedoTex", buffer_textures1[ALBEDO_TEXTURE], 5 + ALBEDO_TEXTURE);
	post_shader.setTexture("EmissionTex", buffer_textures1[EMISSION_TEXTURE], 5 + EMISSION_TEXTURE);
	post_shader.setTexture("NormalTex", buffer_textures1[NORMAL_TEXTURE], 5 + NORMAL_TEXTURE);
	post_shader.setTexture("DepthTex", buffer_textures1[DEPTH_TEXTURE], 5 + DEPTH_TEXTURE);
	post_shader.setTexture("HistoryTex", buffer_textures1[HISTORY_TEXTURE], 5 + HISTORY_TEXTURE);

	glBindVertexArray(vao);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

	std::swap(fbo1, fbo2);
	std::swap(buffer_textures1, buffer_textures2);

	last_camera = camera;
}

bool loadScene(Shader shader, const std::string scene_path, unsigned int* map_texture, unsigned int* bricks_texture, unsigned int* mats_texture) {
	std::ifstream scene_file(kAssetsFolder + scene_path);
	
	// map
	std::string brickmap_path;
	scene_file >> brickmap_path;
	brick_map = std::unique_ptr<BrickMap>(new BrickMap((kAssetsFolder + brickmap_path).c_str()));
	if (brick_map->data.empty()) return false; // failed to load brickmap

	std::vector<std::string> brick_paths;
	std::string next_brick_path;
	while (scene_file >> next_brick_path)
		brick_paths.push_back(next_brick_path);
	
	shader.use();
	shader.setUVec3("MapSize", brick_map->size.x, brick_map->size.y, brick_map->size.z);

	glGenTextures(1, map_texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, *mats_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, brick_map->size.x * brick_map->size.y / 8, brick_map->size.z, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, brick_map->data.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glUniform1i(glGetUniformLocation(shader.ID, "BrickMap"), 0);

	// bricks
	glGenTextures(1, bricks_texture);
	glActiveTexture(GL_TEXTURE0 + 1);
	glBindTexture(GL_TEXTURE_2D_ARRAY, *bricks_texture);

	// allocate bricks texture array
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_R32UI, BRICK_SIZE * BRICK_SIZE / 8, BRICK_SIZE, brick_paths.size(), 0, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);

	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	std::vector<uint32_t> mats_data(brick_paths.size() * 16 * 2);

	for (int i = 0; i < brick_paths.size(); i++)
	{
		bricks.push_back(std::unique_ptr<Brick>(new Brick((kAssetsFolder + brick_paths[i]).c_str())));

		Brick* brick = bricks.back().get();

		if (brick->data.empty()) return false; // failed to load brick

		// assign brick data
		glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, BRICK_SIZE * BRICK_SIZE / 8, BRICK_SIZE, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, brick->data.data());

		for (int j = 1; j < brick->mats.size(); j++) // load all brick's materials
		{
			mats_data[(i * 16 + j) * 2 + 0] = brick->mats[j].color | (brick->mats[j].roughness << 24);
			mats_data[(i * 16 + j) * 2 + 1] = brick->mats[j].emission;
		}
	}

	glUniform1i(glGetUniformLocation(shader.ID, "BricksTex"), 1);

	// materials
	glGenTextures(1, mats_texture);
	glActiveTexture(GL_TEXTURE0 + 2);
	glBindTexture(GL_TEXTURE_2D, *mats_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32UI, 16, brick_paths.size(), 0, GL_RG_INTEGER, GL_UNSIGNED_INT, mats_data.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	glUniform1i(glGetUniformLocation(shader.ID, "MatsTex"), 2);

	shader.setVec3("EnvironmentColor", brick_map->env_color);

	camera = brick_map->camera;

	return true;
}

bool isPositionOccupied(const glm::vec3 pos) {
	if (pos.x < 0.0f || pos.x >= brick_map->size.x || pos.y < 0.0f || pos.y >= brick_map->size.y || pos.z < 0.0f || pos.z >= brick_map->size.z)
		return false;

	unsigned int brick_ID = brick_map->getVoxel(pos.x, pos.y, pos.z);
	
	if (brick_ID == 0) return false; // air

	glm::ivec3 in_brick_pos = glm::ivec3(pos * float(BRICK_SIZE)) % BRICK_SIZE;
	unsigned int voxel_mat = bricks[brick_ID - 1]->getVoxel(in_brick_pos.x, in_brick_pos.y, in_brick_pos.z);

	return voxel_mat != 0;
}