#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <iostream>
#include <glm\glm.hpp>

namespace config {
	const int			WindowStartWidth = 1000;
	const int			WindowStartHeight = 700;

	const char*			WindowName = "My Window";

	const char*			VertexShaderPath = "shaders/vertex.vert";
	const char*			FragShaderPath = "shaders/pathtrace.frag";
	const char*			PostFragShaderPath = "shaders/composite.frag";

	const int			BrickSize = 8;

	const bool			VSYNC = false;

	const char*			OutputNames[9] = { "Result", "Composite", "Illumination", "Albedo", "Emission", "Roughness", "Normal", "Depth", "History"};
	const unsigned int	FPSAverageAmount = 80;

	const float			MaxHighlightDistance = 8.;

	const glm::vec3		SelectedLineColor = glm::vec3(0.);
	const glm::vec3		CrosshairColor = glm::vec3(0.2);
	const float			CrosshairSize = 0.02;
	const float			LineWidth = 2;

	const float			Gamma = 2.2f;
	const int			BlurSize = 1;


	template <class Args>
	static void PrintArgs_(Args args, std::ostream& stream) {
		stream << args << " ";
	}

	template<class... Args>
	void PrintError(Args... args) {
		std::cerr << "ERROR: ";
		int dummy[] = { 0, ((void)PrintArgs_(std::forward<Args>(args), std::cerr), 0)... };
		std::cerr << std::endl;
	}

	template<class... Args>
	void PrintInfo(Args... args) {
		std::cout << "INFO: ";
		int dummy[] = { 0, ((void)PrintArgs_(std::forward<Args>(args), std::cout), 0)... };
		std::cout << std::endl;
	}
}

#endif