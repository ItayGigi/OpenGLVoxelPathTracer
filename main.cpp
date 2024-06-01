#include <iostream>
#include <glad/glad.h> 
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "shader.h"
#include "camera.h"
#include "brick.h"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
void draw(Shader shader, unsigned int VAO);
unsigned int createVAO();
bool loadScene(Shader shader, const char* brickmapPath, const char* brickNames[], const unsigned int brickCount, unsigned int* mapTexture, unsigned int* bricksTexture, unsigned int* matsTexture);
int windowWidth = 800, windowHeight = 600;

// camera
Camera camera(glm::vec3(0.0f, 1.0f, 0.0f));
float lastX = windowWidth / 2.0f;
float lastY = windowHeight / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;	// time between current frame and last frame
float lastFrame = 0.0f;
float lastTitleUpdateTime = 0.f;

int main() {
	// initialize glfw
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// create window
	GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "My Window", NULL, NULL);
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
	glfwSwapInterval(0);

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

	unsigned int sceneTex, bricksTex, matsTex;
	const char* bricks[2] = { "bricks/block.vox", "bricks/chair.vox" };

	if (!loadScene(shader, "map.vox", bricks, 2, &sceneTex, &bricksTex, &matsTex)) {
		std::cerr << "failed to load scene. exiting" << std::endl;
		glDeleteTextures(1, &sceneTex);
		glDeleteTextures(1, &sceneTex);
		glDeleteTextures(1, &matsTex);
		glfwTerminate();
		return -1;
	}

	camera.Yaw = 45;
	camera.Pitch = 45;

	unsigned int frameCount = 0;

	// render loop
	while (!glfwWindowShouldClose(window))
	{
		frameCount++;
		float currentFrame = static_cast<float>(glfwGetTime());
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		lastTitleUpdateTime += deltaTime;

		if (lastTitleUpdateTime >= 0.1f) {
			glfwSetWindowTitle(window, std::to_string((int)(1 / deltaTime)).c_str());
			lastTitleUpdateTime = 0.f;
		}

		processInput(window);

		draw(shader, VAO);

		glfwPollEvents();
		glfwSwapBuffers(window);
	}

	glDeleteTextures(1, &sceneTex);
	glDeleteTextures(1, &sceneTex);
	glDeleteTextures(1, &matsTex);

	glfwTerminate();
	return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
	windowWidth = width;
	windowHeight = height;
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
	shader.use();

	shader.setMat4("CamRotation", glm::mat4_cast(camera.GetRotation()));
	shader.setVec3("CamPosition", camera.Position);

	shader.setUVec2("Resolution", windowWidth, windowHeight);

	glBindVertexArray(VAO);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

bool loadScene(Shader shader, const char* brickmapPath, const char* brickNames[], const unsigned int brickCount, unsigned int* mapTexture, unsigned int* bricksTexture, unsigned int* matsTexture) {
	// map
	BrickMap brickMap((std::string("assets/") + brickmapPath).c_str());
	if (!brickMap.data) return false; // failed to load brickmap

	shader.use();
	shader.setUVec3("MapSize", brickMap.size_x, brickMap.size_y, brickMap.size_z);

	glGenTextures(1, mapTexture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, *matsTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, brickMap.size_x*brickMap.size_y/8, brickMap.size_z, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, brickMap.data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	shader.use();
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
			matsData[(i * 16 + j) * 2 + 0] = brick.mats[j].color >> 8;
			matsData[(i * 16 + j) * 2 + 1] = 0;
		}
	}

	shader.use();
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

	shader.use();
	glUniform1i(glGetUniformLocation(shader.ID, "MatsTex"), 2);

	delete[] matsData;

	return true;
}