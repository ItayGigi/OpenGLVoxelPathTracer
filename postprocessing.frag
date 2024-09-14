#version 330 core

out vec3 FragColor;

in vec2 TexCoord;

uniform uvec2 Resolution;
uniform int OutputNum;
uniform float Gamma;

uniform sampler2D Texture;
uniform sampler2D AlbedoTex;
uniform sampler2D EmissionTex;
uniform sampler2D HistoryTex;
uniform isampler2D NormalTex;
uniform sampler2D DepthTex;


vec3 ACES(const vec3 x) {
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;
	return (x * (a * x + b)) / (x * (c * x + d) + e);
}

vec4 averageSample(sampler2D tex, ivec2 loc){
	return
		texelFetch(tex, loc, 0)*0.6 +
		texelFetch(tex, loc + ivec2(1, 0), 0)*0.1 +
		texelFetch(tex, loc + ivec2(-1, 0), 0)*0.1 +
		texelFetch(tex, loc + ivec2(0, 1), 0)*0.1 +
		texelFetch(tex, loc + ivec2(0, -1), 0)*0.1;
}

void main()
{
	ivec2 pixelLoc = ivec2((TexCoord*0.5+0.5)*Resolution);

	vec3 albedo = averageSample(AlbedoTex, pixelLoc).rgb;

	vec3 incomingLight = texelFetch(Texture, pixelLoc, 0).rgb;
	float emission = texelFetch(EmissionTex, pixelLoc, 0).r;

	switch(OutputNum){
		case 0: // Result
		break;
		case 1: // Composite
		break;

		case 2: // Illumination
		FragColor = incomingLight;
		return;

		case 3: // Albedo
		FragColor = albedo;
		return;

		case 4: // Emission
		FragColor = vec3(emission);
		return;

		case 5: // Normal
		FragColor = texelFetch(NormalTex, pixelLoc, 0).rgb;
		return;

		case 6: // Depth
		FragColor = texelFetch(DepthTex, pixelLoc, 0).rrr/50.;
		return;

		case 7: // History
		FragColor = texelFetch(HistoryTex, pixelLoc, 0).rrr/100.;
		return;
	}

	if (emission == -1.) {
		FragColor = albedo; // no hit
	}
	else {
		FragColor = albedo * (incomingLight + emission);
	}

	if (OutputNum == 0)
		FragColor = ACES(FragColor); // tonemapping

	FragColor = pow(FragColor, vec3(1.0/Gamma)); // gamma correction

	return;
}