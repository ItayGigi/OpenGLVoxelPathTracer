#include <iostream>
#include <glad/glad.h> 
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "shader.h"
#include "camera.h"
#include "brick.h"

#include <queue>

#define VSYNC false

const enum BufferTexture {
	ScreenTexture = 0,
	HistoryTexture,
	DepthTexture,
	AlbedoTexture,
	NormalTexture,
};

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
void draw(Shader shader, unsigned int VAO);
unsigned int createVAO();
bool loadScene(Shader shader, const char* brickmapPath, const char* brickNames[], const unsigned int brickCount, unsigned int* mapTexture, unsigned int* bricksTexture, unsigned int* matsTexture);
void updateFPS(GLFWwindow* window);

// window
GLFWwindow* window;
int windowWidth = 1000, windowHeight = 700;

// camera
Camera camera;
Camera lastCamera;

float lastX = windowWidth / 2.0f;
float lastY = windowHeight / 2.0f;
bool firstMouse = true;

// scene
const char* bricks[3] = { "bricks/frame.vox", "bricks/chair.vox", "bricks/light.vox" };
const char* scenePath = "menger.vox";

//const char* bricks[8] = {
//	"bricks/minecraft/white_concrete.vox",
//	"bricks/minecraft/blue_wool.vox",
//	"bricks/minecraft/light_blue_wool.vox",
//	"bricks/minecraft/lime_wool.vox",
//	"bricks/minecraft/orange_wool.vox",
//	"bricks/minecraft/red_wool.vox",
//	"bricks/minecraft/yellow_wool.vox",
//	"bricks/minecraft/light.vox" };
//const char* scenePath = "minecraft.vox";

// timing
float deltaTime = 0.0f;	// time between current frame and last frame
float lastFrameTime = 0.0f;
unsigned int frameCount = 0;
unsigned int framesSinceLastMoved = 0;

// fps
float frameTimesSum = 0.0f;
std::queue<float> lastFrameTimes;
int fpsAverageAmount = 150;

// frame buffers
unsigned int fbo1, fbo2;
unsigned int bufferTextures1[5], bufferTextures2[5];

int main() {
	// initialize glfw
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// create window
	window = glfwCreateWindow(windowWidth, windowHeight, "My Window", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSwapInterval(VSYNC);

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// initialize glad
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		glfwTerminate();
		return -1;
	}

	unsigned int VAO = createVAO();

	Shader shader("vertex.vert", "fragment.frag");
	Shader postProcessShader("vertex.vert", "postprocessing.frag");

	// load scene
	unsigned int sceneTex, bricksTex, matsTex;
	if (!loadScene(shader, scenePath, bricks, sizeof(bricks) / sizeof(char*), &sceneTex, &bricksTex, &matsTex)) {
		std::cerr << "failed to load scene. exiting" << std::endl;
		glDeleteTextures(1, &sceneTex);
		glDeleteTextures(1, &bricksTex);
		glDeleteTextures(1, &matsTex);
		glfwTerminate();
		return -1;
	}

	glGenFramebuffers(1, &fbo1);
	glGenFramebuffers(1, &fbo2);

	framebuffer_size_callback(window, windowWidth, windowHeight);

	// render loop
	while (!glfwWindowShouldClose(window))
	{
		processInput(window);

		glBindFramebuffer(GL_FRAMEBUFFER, fbo1);
		glClear(GL_COLOR_BUFFER_BIT);

		shader.use();

		shader.setTexture("LastFrameTex", bufferTextures2[ScreenTexture], 5 + ScreenTexture);
		shader.setTexture("HistoryTex", bufferTextures2[HistoryTexture], 5 + HistoryTexture);
		shader.setTexture("LastDepthTex", bufferTextures2[DepthTexture], 5 + DepthTexture);
		shader.setTexture("LastNormalTex", bufferTextures2[NormalTexture], 5 + NormalTexture);

		draw(shader, VAO);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		postProcessShader.use();

		postProcessShader.setTexture("Texture", bufferTextures1[ScreenTexture], 5 + ScreenTexture);
		postProcessShader.setTexture("AlbedoTex", bufferTextures1[AlbedoTexture], 5 + AlbedoTexture);
		
		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

		std::swap(fbo1, fbo2);
		std::swap(bufferTextures1, bufferTextures2);

		glfwPollEvents();
		glfwSwapBuffers(window);
	}

	glDeleteTextures(1, &sceneTex);
	glDeleteTextures(1, &bricksTex);
	glDeleteTextures(1, &matsTex);
	for (int i = 0; i < sizeof(bufferTextures1) / sizeof(unsigned int); i++) {
		glDeleteTextures(1, &bufferTextures1[i]);
		glDeleteTextures(1, &bufferTextures2[i]);
	}

	glfwTerminate();
	return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
	windowWidth = width;
	windowHeight = height;

	unsigned int attachments[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4};

	for (int i = 0; i < 2; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, fbo1);
		// generate textures
		for (int i = 0; i < sizeof(bufferTextures1) / sizeof(unsigned int); i++) {
			glGenTextures(1, &bufferTextures1[i]);
			glActiveTexture(GL_TEXTURE0 + 5 + i);
			glBindTexture(GL_TEXTURE_2D, bufferTextures1[i]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

			glFramebufferTexture2D(GL_FRAMEBUFFER, attachments[i], GL_TEXTURE_2D, bufferTextures1[i], 0);

			glBindTexture(GL_TEXTURE_2D, 0);
		}

		glActiveTexture(GL_TEXTURE0 + 5 + ScreenTexture);
		glBindTexture(GL_TEXTURE_2D, bufferTextures1[ScreenTexture]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, windowWidth, windowHeight, 0, GL_RGB, GL_FLOAT, NULL);

		glActiveTexture(GL_TEXTURE0 + 5 + HistoryTexture);
		glBindTexture(GL_TEXTURE_2D, bufferTextures1[HistoryTexture]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, windowWidth, windowHeight, 0, GL_RED, GL_FLOAT, NULL);

		glActiveTexture(GL_TEXTURE0 + 5 + DepthTexture);
		glBindTexture(GL_TEXTURE_2D, bufferTextures1[DepthTexture]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, windowWidth, windowHeight, 0, GL_RED, GL_FLOAT, NULL);

		glActiveTexture(GL_TEXTURE0 + 5 + AlbedoTexture);
		glBindTexture(GL_TEXTURE_2D, bufferTextures1[AlbedoTexture]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, windowWidth, windowHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

		glActiveTexture(GL_TEXTURE0 + 5 + NormalTexture);
		glBindTexture(GL_TEXTURE_2D, bufferTextures1[NormalTexture]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8I, windowWidth, windowHeight, 0, GL_RGB_INTEGER, GL_INT, NULL);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;

		glDrawBuffers(sizeof(attachments)/sizeof(unsigned int), attachments);

		std::swap(fbo1, fbo2);
		std::swap(bufferTextures1, bufferTextures2);
	}

	glBindBuffer(GL_FRAMEBUFFER, 0);
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
void processInput(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		camera.ProcessKeyboard(FORWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		camera.ProcessKeyboard(BACKWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		camera.ProcessKeyboard(LEFT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		camera.ProcessKeyboard(RIGHT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
		camera.ProcessKeyboard(UP, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
		camera.ProcessKeyboard(DOWN, deltaTime);
}

// glfw: whenever the mouse moves, this callback is called
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
	float xpos = static_cast<float>(xposIn);
	float ypos = static_cast<float>(yposIn);

	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

	lastX = xpos;
	lastY = ypos;

	camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

void updateFPS(GLFWwindow* window) {
	lastFrameTimes.push(deltaTime);
	frameTimesSum += deltaTime;

	if (frameCount > fpsAverageAmount) {
		frameTimesSum -= lastFrameTimes.front();
		lastFrameTimes.pop();

		float average = fpsAverageAmount / frameTimesSum;
		glfwSetWindowTitle(window, std::to_string(average).c_str());
	}
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

void draw(Shader shader, unsigned int VAO) {
	frameCount++;
	framesSinceLastMoved++;
	float currentFrameTime = static_cast<float>(glfwGetTime());
	deltaTime = currentFrameTime - lastFrameTime;
	lastFrameTime = currentFrameTime;

	updateFPS(window);

	if (camera.Position != lastCamera.Position || camera.Front != lastCamera.Front) {
		framesSinceLastMoved = 0;
	}

	shader.use();

	shader.setMat4("CamRotation", glm::mat4_cast(camera.GetRotation()));
	shader.setVec3("CamPosition", camera.Position);

	shader.setMat4("LastCamRotation", glm::mat4_cast(lastCamera.GetRotation()));
	shader.setVec3("LastCamPosition", lastCamera.Position);

	shader.setUVec2("Resolution", windowWidth, windowHeight);

	shader.setUInt("FrameCount", frameCount);
	shader.setUInt("AccumulatedFramesCount", framesSinceLastMoved);

	glBindVertexArray(VAO);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

	lastCamera = camera;
}

bool loadScene(Shader shader, const char* brickmapPath, const char* brickNames[], const unsigned int brickCount, unsigned int* mapTexture, unsigned int* bricksTexture, unsigned int* matsTexture) {
	// map
	Scene brickMap((std::string("assets/") + brickmapPath).c_str());
	if (!brickMap.data) return false; // failed to load brickmap

	shader.use();
	shader.setUVec3("MapSize", brickMap.size_x, brickMap.size_y, brickMap.size_z);

	glGenTextures(1, mapTexture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, *matsTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, brickMap.size_x*brickMap.size_y/8, brickMap.size_z, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, brickMap.data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glUniform1i(glGetUniformLocation(shader.ID, "BrickMap"), 0);
	
	// bricks
	glGenTextures(1, bricksTexture);
	glActiveTexture(GL_TEXTURE0 + 1);
	glBindTexture(GL_TEXTURE_2D_ARRAY, *bricksTexture);

	// allocate bricks texture array
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_R32UI,	BRICK_SIZE * BRICK_SIZE / 8, BRICK_SIZE, brickCount, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);

	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	uint32_t* matsData = new uint32_t[brickCount * 16 * 2]();

	for (int i = 0; i < brickCount; i++)
	{
		Brick brick((std::string("assets/") + brickNames[i]).c_str());

		if (!brick.data) return false; // failed to load brick

		// assign brick data
		glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, BRICK_SIZE, BRICK_SIZE * BRICK_SIZE / 8, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, brick.data);

		for (int j = 1; j < brick.matCount; j++) // load all brick's materials
		{
			matsData[(i * 16 + j) * 2 + 0] = brick.mats[j].color | (brick.mats[j].roughness << 24);
			matsData[(i * 16 + j) * 2 + 1] = brick.mats[j].emission;
		}
	}

	glUniform1i(glGetUniformLocation(shader.ID, "BricksTex"), 1);

	// materials
	glGenTextures(1, matsTexture);
	glActiveTexture(GL_TEXTURE0 + 2);
	glBindTexture(GL_TEXTURE_2D, *matsTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32UI, 16, brickCount, 0, GL_RG_INTEGER, GL_UNSIGNED_INT, matsData);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	glUniform1i(glGetUniformLocation(shader.ID, "MatsTex"), 2);

	delete[] matsData;

	shader.setVec3("EnvironmentColor", brickMap.env_color);

	camera = brickMap.camera;

	return true;
}