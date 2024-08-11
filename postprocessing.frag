#version 330 core

out vec3 FragColor;

in vec2 TexCoord;
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

void main()
{
	vec3 color = texture(Texture, TexCoord*0.5+0.5).rgb;
	color += texture(EmissionTex, TexCoord*0.5+0.5).rrr;
	color *= texture(AlbedoTex, TexCoord*0.5+0.5).rgb; // multiply by albedo
	color = ACES(color); // tonemapping
	color = pow(color, vec3(1.0/2.2)); // gamma correction
	FragColor = color;
	
	//FragColor = texture(Texture, TexCoord*0.5+0.5).rgb;;
	return;
}