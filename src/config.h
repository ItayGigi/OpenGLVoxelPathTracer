#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <glm\glm.hpp>

namespace config {
	const int			WindowStartWidth = 1000;
	const int			WindowStartHeight = 700;

	const char*			WindowName = "My Window";

	const char*			VertexShaderPath = "shaders/vertex.vert";
	const char*			FragShaderPath = "shaders/fragment.frag";
	const char*			PostFragShaderPath = "shaders/postprocess.frag";

	const int			BrickSize = 8;

	const bool			VSYNC = false;

	const char*			OutputNames[8] = { "Result", "Composite", "Illumination", "Albedo", "Emission", "Normal", "Depth", "History" };
	const unsigned int	FPSAverageAmount = 80;

	const float			MaxHighlightDistance = 8.;

	const glm::vec3		SelectedLineColor = glm::vec3(0.);
	const glm::vec3		CrosshairColor = glm::vec3(0.2);
	const float			CrosshairSize = 0.02;
	const float			LineWidth = 2;

	const float			Gamma = 2.2f;
	const int			BlurSize = 1;
}

#endif