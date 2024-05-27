#version 330 core

out vec4 FragColor;

in vec2 TexCoord;
uniform uvec2 Resolution;
uniform mat4 View;
uniform mat4 Projection;
uniform mat4 CamRotation;
uniform vec3 CamPosition;

uniform usampler2D GridTex;

struct Ray{
	vec3 origin;
	vec3 dir;
};

struct Material{
	vec3 color;
	float smoothness;
	float emission;
};

struct Sphere{
	vec3 pos;
	float radius;
	Material mat;
};

uint GetGridCell(usampler2D tex, int x, int y, int z){
	if (x < 0 || x >= 8 || y < 0 || y >= 8 || z < 0 || z >= 8) return 0u;

	uint row = texture(tex, vec2(float(x)/8., float(y)/8.)).r;
	return (row >> z*4) & 0xFu;
}

float RaySphereIntersection(Ray ray, Sphere sphere){
	vec3 origin = ray.origin - sphere.pos;

	float a = dot(ray.dir, ray.dir);
	float b = 2*dot(origin, ray.dir);
	float c = dot(origin, origin) - sphere.radius*sphere.radius;

	float discriminant = b*b - 4*a*c;

	if (discriminant < 0) return -1.;

	return (-b - sqrt(discriminant))/(2*a);
}

float RaySlabIntersection(Ray ray, vec3 min, vec3 max){
	float tmin = (min.x - ray.origin.x) / ray.dir.x; 
	float tmax = (max.x - ray.origin.x) / ray.dir.x; 

	if (tmin > tmax) {
		float temp = tmin;
		tmin = tmax;
		tmax = temp;
	}

	float tymin = (min.y - ray.origin.y) / ray.dir.y; 
	float tymax = (max.y - ray.origin.y) / ray.dir.y; 

	if (tymin > tymax) {
		float temp = tymin;
		tymin = tymax;
		tymax = temp;
	}

	if ((tmin > tymax) || (tymin > tmax)) 
			return -1.; 

	if (tymin > tmin) 
			tmin = tymin; 

	if (tymax < tmax) 
			tmax = tymax; 

	float tzmin = (min.z - ray.origin.z) / ray.dir.z; 
	float tzmax = (max.z - ray.origin.z) / ray.dir.z; 

	if (tzmin > tzmax) {
		float temp = tzmin;
		tzmin = tzmax;
		tzmax = temp;
	}

	if ((tmin > tzmax) || (tzmin > tmax)) 
			return -1.; 

	if (tzmin > tmin) 
			tmin = tzmin; 

	if (tzmax < tmax) 
			tmax = tzmax;

	return tmin;
}

float RayGridIntersection(Ray ray){
	float boundT = RaySlabIntersection(ray, vec3(0.), vec3(1.));

	float stepx = sign(ray.dir.x);
	float stepy = sign(ray.dir.y);
	float stepz = sign(ray.dir.z);

	return boundT;
}

vec4 TestGrid(usampler2D grid, vec2 coords){
	if (coords.y > 0.) return vec4(GetGridCell(GridTex, int(fract(coords.x*4.+4.)*8.), int(coords.y*32.), int(coords.x * 4. + 4.))/8.);
	else if (coords.y > -1/4.) return vec4(int(fract(coords.x*4.+4.)*8.)/8., int(coords.y*32.+8.)/8., int(coords.x * 4. + 4.)/8., 1.);
	else return vec4(0.);
}

void main()
{
	float aspect = float(Resolution.x)/float(Resolution.y);

	vec3 dir = (CamRotation * vec4(TexCoord.x*aspect, TexCoord.y, 1., 1.)).xyz;

	Ray ray = Ray(CamPosition, normalize(dir));
	Sphere sphere = Sphere(vec3(0., 1., 0.), 1., Material(vec3(1.), 0., 0.));

	//float dist = RaySphereIntersection(ray, sphere);
	float dist = RaySlabIntersection(ray, vec3(0.), vec3(1.));

	if (dist < 0){
		FragColor = vec4(0., 0., 0., 1.);
		return;
	}

	FragColor = vec4(ray.origin+ray.dir*dist, 1.0f);
}