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
#define SAMPLES 10

uint ns;
//#define INIT_RNG ns = uint(frame)*uint(Resolution.x*Resolution.y)+uint(TexCoord.x+TexCoord.y*Resolution.x)
#define INIT_RNG ns = uint(Resolution.x*Resolution.y)+uint((0.5*TexCoord.x+0.5)*Resolution.x+(0.5*TexCoord.y+0.5)*Resolution.x*Resolution.y)

// PCG Random Number Generator
void pcg()
{
		uint state = ns*747796405U+2891336453U;
		uint word  = ((state >> ((state >> 28U) + 4U)) ^ state)*277803737U;
		ns = (word >> 22U) ^ word;
}

// Random Floating-Point Scalars/Vectors
float rand(){pcg(); return float(ns)/float(0xffffffffU);}
vec2 rand2(){return vec2(rand(), rand());}
vec3 rand3(){return vec3(rand(), rand(), rand());}
vec4 rand4(){return vec4(rand(), rand(), rand(), rand());}

float RandomNumberNormalDist(){
	float theta = 2 * 3.1415926 * rand();
	float rho = sqrt(-2 * log(rand()));
	return rho * cos(theta);
}

vec2 SampleDisc(vec2 uv)
{
	float theta = 2.0 * 3.141592653589 * uv.x;
	float r = sqrt(uv.y);
	return vec2(cos(theta), sin(theta)) * r;
}

vec3 RandomInHemisphere(vec2 uv)
{
	vec2 disk = SampleDisc(uv);
	return vec3(disk.x, sqrt(max(0.0, 1.0 - dot(disk, disk))), disk.y);
}

vec3 CosWeightedRandomHemisphereDirection( const vec3 n ) {
  lowp vec2 r = rand2();
	lowp vec3  uu = normalize( cross( n, vec3(0.0,1.0,1.0) ) );
	lowp vec3  vv = cross( uu, n );
	lowp float ra = sqrt(r.y);
	lowp float rx = ra*cos(6.2831*r.x); 
	lowp float ry = ra*sin(6.2831*r.x);
	lowp float rz = sqrt( 1.0-r.y );
	lowp vec3  rr = vec3( rx*uu + ry*vv + rz*n );
  return normalize( rr );
}

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

uint GetBrickMapCell(ivec3 loc){
	uint row = texelFetch(BrickMap, ivec2(loc.x*int(MapSize.y)/8 + loc.y/8, loc.z), 0).r;
	return (row >> (loc.y%8)*4) & 0xFu;
}

uint GetBrickCell(int brick, ivec3 loc){
	uint row = texelFetch(BricksTex, ivec3(loc.x, loc.z, brick-1), 0).r;
	return (row >> loc.y*4) & 0xFu;
}

Material GetMaterial(int brickIndex, int matIndex){
	uvec4 val = texelFetch(MatsTex, ivec2(matIndex, brickIndex), 0);
	vec3 color = vec3(float((val.r >> 16) & 0xFFu)/255., float((val.r >> 8) & 0xFFu)/255., float((val.r >> 0) & 0xFFu)/255.);
	float emission = (val.g & 0xFFFFu)/5.;
	return Material(color, 1., emission);
}

vec4 TestBrick(int brick, vec2 coords){
	if (coords.y > 0.) return vec4(GetBrickCell(brick, ivec3(int(fract(coords.x*4.+4.)*8.), int(coords.y*32.), int(coords.x * 4. + 4.)))/8.);
	else if (coords.y > -1/4.) return vec4(int(fract(coords.x*4.+4.)*8.)/8., int(coords.y*32.+8.)/8., int(coords.x * 4. + 4.)/8., 1.);
	else return vec4(0.);
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
GridHit RayBrickIntersection(Ray ray, int brickIndex, vec3 gridPos, float gridScale){
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
		uint cell = GetBrickCell(brickIndex, curr_voxel);
		if (cell != 0u)
			return GridHit(true, dist + max(tMin, 0.), -ivec3(mask)*step, GetMaterial(brickIndex-1, int(cell)), iter);

		mask = lessThanEqual(t_next.xyz, min(t_next.yzx, t_next.zxy));

		dist = min(min(t_next.x, t_next.y), t_next.z);
		t_next += vec3(mask) * t_delta;
		curr_voxel += ivec3(mask) * step;
	}

	uint cell = GetBrickCell(brickIndex, curr_voxel);
	if (cell != 0u)
		return GridHit(true, dist + max(tMin, 0.), -ivec3(mask)*step, GetMaterial(brickIndex-1, int(cell)), iter);
	
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

vec3 Trace(Ray ray){
	vec3 rayColor = vec3(1.);
	vec3 incomingLight = vec3(0.);

	for (int i=0; i < 6; i++){
		GridHit hitInfo = RaySceneIntersection(ray, vec3(0.), 1.);

		if (!hitInfo.hit){
			vec3 skyColor = vec3(0.07);
			return incomingLight + rayColor * skyColor;
		}

		if (hitInfo.mat.emission == 0.)
			rayColor *= hitInfo.mat.color;
		else
			incomingLight += rayColor * hitInfo.mat.emission * hitInfo.mat.color;

		ray.origin += ray.dir*hitInfo.dist + hitInfo.normal*EPSILON;
		
		vec3 diffuseDir = CosWeightedRandomHemisphereDirection(hitInfo.normal);

		vec3 specularDir = reflect(ray.dir, vec3(hitInfo.normal));

		ray.dir = normalize(mix(specularDir, diffuseDir, hitInfo.mat.roughness));
		ray.inverse_dir = 1.0/ray.dir;
	}

	return incomingLight;
}

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
	INIT_RNG;

	float aspect = float(Resolution.x)/float(Resolution.y);

	vec3 sumColor = vec3(0.);
	for (int s = 0; s < SAMPLES; s++) {
		vec3 dir = normalize((CamRotation * vec4(TexCoord.x*aspect + (2.*rand()-1.)/Resolution.y, TexCoord.y + (2.*rand()-1.)/Resolution.y, 1.5, 1.)).xyz);
		Ray ray = Ray(CamPosition, dir, 1.0/dir);

		sumColor += Trace(ray);
	}

	vec3 color = ACES(sumColor/SAMPLES); // tonemapping
	color = pow(color, vec3(1.0/2.2)); // gamma correction

	FragColor = vec4(color, 1.);
	return;
}