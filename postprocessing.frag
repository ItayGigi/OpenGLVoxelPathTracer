#version 330 core

out vec3 FragColor;

in vec2 TexCoord;

uniform uvec2 Resolution;
uniform sampler2D Texture;
uniform sampler2D AlbedoTex;
uniform sampler2D EmissionTex;

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
		texelFetch(tex, loc, 0)*0.4 +
		texelFetch(tex, loc + ivec2(1, 0), 0)*0.15 +
		texelFetch(tex, loc + ivec2(-1, 0), 0)*0.15 +
		texelFetch(tex, loc + ivec2(0, 1), 0)*0.15 +
		texelFetch(tex, loc + ivec2(0, -1), 0)*0.15;
}

void main()
{
	ivec2 pixelLoc = ivec2((TexCoord*0.5+0.5)*Resolution);

	vec3 color = texelFetch(Texture, pixelLoc, 0).rgb;

	color += texelFetch(EmissionTex, pixelLoc, 0).rrr; // add emission

	color *= averageSample(AlbedoTex, pixelLoc).rgb; // multiply by albedo

	color = ACES(color); // tonemapping
	color = pow(color, vec3(1.0/2.2)); // gamma correction
	FragColor = color;
	
	//FragColor = averageSample(AlbedoTex, pixelLoc).rgb;

	//FragColor = texture(Texture, TexCoord*0.5+0.5).rgb;
	return;
}