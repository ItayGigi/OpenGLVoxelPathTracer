#include <iostream>
#include <glad/glad.h> 
#include <GLFW/glfw3.h>
#include "config.h"
#include "renderer.h"

Renderer renderer;

int main(int argc, const char* argv[]) {
	// initialize glfw
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// create window
	GLFWwindow* window = glfwCreateWindow(config::WindowStartWidth, config::WindowStartHeight, config::WindowName, NULL, NULL);
	if (window == NULL)
	{
		config::PrintError("Failed to create GLFW window");
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);

	glfwSetFramebufferSizeCallback(window,
		[](GLFWwindow* window, int width, int height) {
			renderer.HandleFramebufferSizeCallback(window, width, height); });
	glfwSetCursorPosCallback(window,
		[](GLFWwindow* window, double x_pos_in, double y_pos_in) {
			renderer.HandleMouseCallback(window, x_pos_in, y_pos_in); });
	glfwSetScrollCallback(window,
		[](GLFWwindow * window, double x_offset, double y_offset) {
			renderer.HandleScrollCallback(window, x_offset, y_offset); });
	glfwSetMouseButtonCallback(window,
		[](GLFWwindow* window, int button, int action, int mods) {
			renderer.HandleMouseButtonCallback(window, button, action, mods); });

	// initialize glad
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		config::PrintError("Failed to initialize GLAD");
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