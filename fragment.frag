#version 330 core

out vec4 FragColor;

in vec2 TexCoord;
uniform uvec2 Resolution;
uniform mat4 View;
uniform mat4 Projection;
uniform mat4 CamRotation;
uniform vec3 CamPosition;

uniform usampler2D GridTex;

#define BRICK_RES 8
#define EPSILON 0.00001

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
	if (x < 0 || x >= BRICK_RES || y < 0 || y >= BRICK_RES || z < 0 || z >= BRICK_RES) return 0u;

	uint row = texture(tex, vec2(float(x)/float(BRICK_RES), float(y)/float(BRICK_RES))).r;
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

struct SlabIntersection{bool hit; float tmin; float tmax;};

SlabIntersection RaySlabIntersection(Ray ray, vec3 min, vec3 max){
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
			return SlabIntersection(false, 0., 0.); 

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
			return SlabIntersection(false, 0., 0.); 

	if (tzmin > tmin) 
			tmin = tzmin; 

	if (tzmax < tmax) 
			tmax = tzmax;

	return SlabIntersection(tmax>0., tmin, tmax);
}

struct GridHit{
	bool hit;
	float dist;
	ivec3 voxelIndex;
};

// J. Amanatides, A. Woo. A Fast Voxel Traversal Algorithm for Ray Tracing.
GridHit RayGridIntersection(Ray ray, usampler2D grid, vec3 gridPos, float gridScale){
	SlabIntersection boundHit = RaySlabIntersection(ray, gridPos, gridPos + vec3(gridScale));
	if (!boundHit.hit) return GridHit(false, -1., ivec3(-1));

	float tMin = boundHit.tmin;
	float tMax = boundHit.tmax;

	vec3 ray_start = ray.origin + ray.dir * tMin - gridPos;
	if (tMin < 0.) ray_start = ray.origin - gridPos;
	vec3 ray_end = ray.origin + ray.dir * tMax - gridPos;

	float voxel_size = gridScale/BRICK_RES;

	ivec3 curr_voxel = max(min(ivec3((ray_start)/voxel_size), ivec3(BRICK_RES-1)), ivec3(0));
	ivec3 last_voxel = max(min(ivec3((ray_end)/voxel_size), ivec3(BRICK_RES-1)), ivec3(0));

	//return GridHit(true, distance(ray.origin, gridPos + (curr_voxel*gridScale)/BRICK_RES), last_voxel);


	vec3 step = sign(ray.dir);

	vec3 t_next = ((curr_voxel+max(step, ivec3(0)))*voxel_size-ray_start)/ray.dir;
	vec3 t_delta = voxel_size/abs(ray.dir);

	int iter = 0;
	while(last_voxel != curr_voxel && iter++ < BRICK_RES*4) {
		if (GetGridCell(grid, curr_voxel.x, curr_voxel.y, curr_voxel.z) != 0u)
			return GridHit(true, distance(ray.origin, gridPos + (curr_voxel*gridScale)/BRICK_RES), curr_voxel);

		if (t_next.x < t_next.y) {
			if (t_next.x < t_next.z) {
				curr_voxel.x += int(step.x);
				t_next.x += t_delta.x;
			} else {
				curr_voxel.z += int(step.z);
				t_next.z += t_delta.z;
			}
		} else {
			if (t_next.y < t_next.z) {
				curr_voxel.y += int(step.y);
				t_next.y += t_delta.y;
			} else {
				curr_voxel.z += int(step.z);
				t_next.z += t_delta.z;
			}
		}
	}

	if (GetGridCell(grid, curr_voxel.x, curr_voxel.y, curr_voxel.z) != 0u)
		return GridHit(true, distance(ray.origin, gridPos + (curr_voxel*gridScale)/BRICK_RES), curr_voxel);
	
	return GridHit(false, iter, last_voxel);
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

	GridHit hit = RayGridIntersection(ray, GridTex, vec3(0.), 1.);

	if (!hit.hit){
		FragColor = vec4(0., 0., 0., 1.);
		return;
	}

	//FragColor = vec4(ray.origin+ray.dir*hit.dist, 1.0f);
	FragColor = vec4(hit.voxelIndex/8., 1.);
}