#include <glad/glad.h> 
#include <GLFW/glfw3.h>
#include "config.h"
#include "renderer.h"

void framebufferSizeCallback(GLFWwindow* window, int width, int height);
void mouseCallback(GLFWwindow* window, double x_pos, double y_pos);
void scrollCallback(GLFWwindow* window, double x_offset, double y_offset);
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

Renderer renderer;

int main(int argc, const char* argv[]) {
	// initialize glfw
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// create window
	GLFWwindow* window = glfwCreateWindow(config::WindowStartWidth, config::WindowStartHeight, config::WindowName, NULL, NULL);
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
	glfwSetMouseButtonCallback(window, mouseButtonCallback);

	// initialize glad
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		glfwTerminate();
		return -1;
	}

	int init_status = renderer.Initialize(window, argc, argv);
	if (init_status != 0) return init_status;

	renderer.HandleFramebufferSizeCallback(window, config::WindowStartWidth, config::WindowStartHeight);

	// render loop
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		renderer.Update(window);

		glfwSwapBuffers(window);
	}

	renderer.Terminate();
	glfwTerminate();

	return 0;
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
	renderer.HandleFramebufferSizeCallback(window, width, height);
}

// glfw: whenever the mouse moves, this callback is called
void mouseCallback(GLFWwindow* window, double x_pos_in, double y_pos_in)
{
	renderer.HandleMouseCallback(window, x_pos_in, y_pos_in);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
void scrollCallback(GLFWwindow* window, double x_offset, double y_offset)
{
	renderer.HandleScrollCallback(window, x_offset, y_offset);
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	renderer.HandleMouseButtonCallback(window, button, action, mods);
}