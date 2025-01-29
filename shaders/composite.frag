#version 330 core

out vec3 FragColor;

in vec2 TexCoord;

uniform uvec2 Resolution;
uniform int OutputNum;
uniform float Gamma;
uniform int BlurSize;

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

vec4 boxBlurIllumination(ivec2 loc, int size, int seperation){
	vec4 accu = vec4(0.);
	ivec4 normal = texelFetch(NormalTex, loc, 0);
	float depth = texelFetch(DepthTex, loc, 0).r;
	int sampleCount = 0;

	for (int i = -size; i <= size; i++){
			for (int j = -size; j <= size; j++){
				ivec2 currLoc = loc + ivec2(i, j) * seperation;
				
				if (texelFetch(NormalTex, currLoc, 0) == normal &&
						abs(texelFetch(DepthTex, currLoc, 0).r - depth) < 0.1){
					sampleCount++;
					accu += texelFetch(Texture, currLoc, 0);
				}
			}
	}

	return accu / sampleCount;
}

void main()
{
	ivec2 pixelLoc = ivec2((TexCoord*0.5+0.5)*Resolution);

	vec3 albedo = texelFetch(AlbedoTex, pixelLoc, 0).rgb; //averageSample(AlbedoTex, pixelLoc).rgb;

	vec3 incomingLight = boxBlurIllumination(pixelLoc, BlurSize, 2).rgb;//texelFetch(Texture, pixelLoc, 0).rgb;
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
		FragColor = texelFetch(DepthTex, pixelLoc, 0).rrr/10.;
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