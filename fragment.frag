#version 330 core

out vec4 FragColor;

in vec2 TexCoord;
uniform uvec2 Resolution;
uniform uvec3 MapSize;

uniform mat4 CamRotation;
uniform vec3 CamPosition;

uniform usampler2D BrickMap;

uniform usampler2DArray BricksTex;
uniform usampler2D MatsTex;

#define BRICK_RES 8
#define EPSILON 0.00001

struct Ray{
	vec3 origin;
	vec3 dir;
	vec3 inverse_dir;
};

struct Material{
	vec3 color;
	float roughness;
	float emission;
};

struct Sphere{
	vec3 pos;
	float radius;
	Material mat;
};

uint GetBrickMapCell(ivec3 loc){
	uint row = texelFetch(BrickMap, ivec2(loc.x*int(MapSize.y)/8 + loc.y/8, loc.z), 0).r;
	return (row >> (loc.y%8)*4) & 0xFu;
}

uint GetBrickCell(int brick, ivec3 loc){
	uint row = texelFetch(BricksTex, ivec3(loc.x, loc.z, brick-1), 0).r;
	return (row >> loc.y*4) & 0xFu;
}

Material GetMaterial(int brickIndex, int matIndex){
	uint val = texelFetch(MatsTex, ivec2(matIndex, brickIndex), 0).r;
	vec3 color = vec3(float((val >> 16) & 0xFFu)/255., float((val >> 8) & 0xFFu)/255., float((val >> 0) & 0xFFu)/255.);

	return Material(color, 0., 0.);
}

struct SlabIntersection{bool hit; float tmin; float tmax; bvec3 normal;};

SlabIntersection RaySlabIntersection(Ray ray, vec3 minpos, vec3 maxpos){
  vec3 tbot = ray.inverse_dir * (minpos - ray.origin);
  vec3 ttop = ray.inverse_dir * (maxpos - ray.origin);

  vec3 dmin = min(ttop, tbot);
  vec3 dmax = max(ttop, tbot);

  float tmin = max(max(dmin.x, dmin.y), dmin.z);
  float tmax = min(min(dmax.x, dmax.y), dmax.z);

	bvec3 normal = equal(dmin, vec3(tmin));

	return SlabIntersection(tmax > max(tmin, 0.0), tmin, tmax, normal);
}

struct GridHit{
	bool hit;
	float dist;
	ivec3 normal;
	Material mat;
	int additional;
};

// J. Amanatides, A. Woo. A Fast Voxel Traversal Algorithm for Ray Tracing.
GridHit RayBrickIntersection(Ray ray, int grid, vec3 gridPos, float gridScale){
	GridHit noHit = GridHit(false, -1., ivec3(-1), Material(vec3(0.), 0., 0.), 0);

	SlabIntersection boundHit = RaySlabIntersection(ray, gridPos, gridPos + vec3(gridScale));
	if (!boundHit.hit) return noHit;

	float tMin = boundHit.tmin;
	float tMax = boundHit.tmax;

	vec3 ray_start = ray.origin + ray.dir * tMin - gridPos;
	if (tMin < 0.) ray_start = ray.origin - gridPos;
	vec3 ray_end = ray.origin + ray.dir * tMax - gridPos;

	float voxel_size = gridScale/BRICK_RES;

	ivec3 curr_voxel = max(min(ivec3((ray_start)/voxel_size), ivec3(BRICK_RES-1)), ivec3(0));
	ivec3 last_voxel = max(min(ivec3((ray_end)/voxel_size), ivec3(BRICK_RES-1)), ivec3(0));

	ivec3 step = ivec3(sign(ray.dir));

	vec3 t_next = ((curr_voxel+max(step, ivec3(0)))*voxel_size-ray_start)*ray.inverse_dir;
	vec3 t_delta = voxel_size*abs(ray.inverse_dir);

	float dist = 0.;
	bvec3 mask = boundHit.normal;

	int iter = 0;
	while(last_voxel != curr_voxel && iter++ < BRICK_RES*4) {
		uint cell = GetBrickCell(grid, curr_voxel);
		if (cell != 0u)
			return GridHit(true, dist + max(tMin, 0.), -ivec3(mask)*step, GetMaterial(grid-1, int(cell)), iter);

		mask = lessThanEqual(t_next.xyz, min(t_next.yzx, t_next.zxy));

		t_next += vec3(mask) * t_delta;
		curr_voxel += ivec3(mask) * step;
		dist = min(min(t_next.x, t_next.y), t_next.z);
	}

	uint cell = GetBrickCell(grid, curr_voxel);
	if (cell != 0u)
		return GridHit(true, dist + max(tMin, 0.), -ivec3(mask)*step, GetMaterial(grid-1, int(cell)), iter);
	
	noHit.additional = iter;
	return noHit;
}

GridHit RaySceneIntersection(Ray ray, vec3 gridPos, float gridScale){
	GridHit noHit = GridHit(false, -1., ivec3(-1), Material(vec3(0.), 0., 0.), 0);

	SlabIntersection boundHit = RaySlabIntersection(ray, gridPos, gridPos + MapSize*gridScale);
	if (!boundHit.hit) return noHit;

	float tMin = boundHit.tmin;
	float tMax = boundHit.tmax;

	vec3 ray_start = ray.origin + ray.dir * tMin - gridPos;
	if (tMin < 0.) ray_start = ray.origin - gridPos;
	vec3 ray_end = ray.origin + ray.dir * tMax - gridPos;

	float voxel_size = gridScale;

	ivec3 curr_voxel = max(min(ivec3((ray_start)/voxel_size), ivec3(MapSize)-ivec3(1)), ivec3(0));
	ivec3 last_voxel = max(min(ivec3((ray_end)/voxel_size), ivec3(MapSize)-ivec3(1)), ivec3(0));

	ivec3 step = ivec3(sign(ray.dir));

	vec3 t_next = ((curr_voxel+max(step, ivec3(0)))*voxel_size-ray_start)*ray.inverse_dir;
	vec3 t_delta = voxel_size*abs(ray.inverse_dir);

	float dist = 0.;
	bvec3 mask;

	int iter = 0, brickIter = 0;
	while(last_voxel != curr_voxel && iter++ < int(MapSize.x + MapSize.y + MapSize.z)) {
		uint cell = GetBrickMapCell(curr_voxel);
		if (cell != 0u){
			GridHit hit = RayBrickIntersection(ray, int(cell), gridPos + curr_voxel*gridScale, gridScale);
			brickIter += hit.additional;
			if (hit.hit) return GridHit(true, hit.dist, hit.normal, hit.mat, iter + brickIter);
		}

		mask = lessThanEqual(t_next.xyz, min(t_next.yzx, t_next.zxy));

		t_next += vec3(mask) * t_delta;
		curr_voxel += ivec3(mask) * step;
		dist = min(min(t_next.x, t_next.y), t_next.z);
	}

	uint cell = GetBrickMapCell(curr_voxel);
	if (cell != 0u){
		GridHit hit = RayBrickIntersection(ray, int(cell), gridPos + curr_voxel*gridScale, gridScale);
		brickIter += hit.additional;
		if (hit.hit) return GridHit(true, hit.dist, hit.normal, hit.mat, iter + brickIter);
	}
	
	noHit.additional = iter + brickIter;
	return noHit;
}

vec4 TestBrick(int brick, vec2 coords){
	if (coords.y > 0.) return vec4(GetBrickCell(brick, ivec3(int(fract(coords.x*4.+4.)*8.), int(coords.y*32.), int(coords.x * 4. + 4.)))/8.);
	else if (coords.y > -1/4.) return vec4(int(fract(coords.x*4.+4.)*8.)/8., int(coords.y*32.+8.)/8., int(coords.x * 4. + 4.)/8., 1.);
	else return vec4(0.);
}

void main()
{
	float aspect = float(Resolution.x)/float(Resolution.y);

	vec3 dir = (CamRotation * vec4(TexCoord.x*aspect, TexCoord.y, 1.5, 1.)).xyz;

	Ray ray = Ray(CamPosition, normalize(dir), 1.0/normalize(dir));

	GridHit hit = RaySceneIntersection(ray, vec3(0.), 1.);

	//FragColor = vec4(hit.additional/100.);
	//return;

	if (!hit.hit){
		vec4 topColor = vec4(0.34, 0.34, 0.34, 1.0);
		vec4 bottomColor = vec4(0.0, 0.0, 0.0, 1.0);
		FragColor = mix(bottomColor, topColor, atan(ray.dir.y)/2*3.14159);
		return;
	}

	//FragColor = vec4(hit.voxelIndex/8., 1.);
	//FragColor = vec4((ray.origin + ray.dir*hit.dist)/10., 1.);
	float light = dot(hit.normal, normalize(vec3(1.5, 2., 1.)))/2.+0.5;
	FragColor = vec4(light*hit.mat.color, 1.);
}