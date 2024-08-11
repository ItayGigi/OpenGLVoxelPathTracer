#version 330 core

layout (location = 0) out vec3 FragColor;
layout (location = 1) out float FragHistory;
layout (location = 2) out float FragDepth;
layout (location = 3) out vec3 FragAlbedo;
layout (location = 4) out ivec3 FragNormal;
layout (location = 5) out float FragEmission;

in vec2 TexCoord;
uniform sampler2D LastFrameTex;
uniform sampler2D HistoryTex;
uniform sampler2D LastDepthTex;
uniform isampler2D LastNormalTex;

uniform uvec2 Resolution;
uniform uint FrameCount;

uniform uvec3 MapSize;

uniform mat4 CamRotation;
uniform vec3 CamPosition;

uniform mat4 LastCamRotation;
uniform vec3 LastCamPosition;

uniform usampler2D BrickMap;

uniform usampler2DArray BricksTex;
uniform usampler2D MatsTex;

uniform vec3 EnvironmentColor;

#define BRICK_RES 8
#define EPSILON 0.00001
#define SAMPLES 1.
#define MAX_BOUNCES 6

uint ns;
//#define INIT_RNG ns = uint(frame)*uint(Resolution.x*Resolution.y)+uint(TexCoord.x+TexCoord.y*Resolution.x)
#define INIT_RNG ns = FrameCount*uint(Resolution.x*Resolution.y)+uint((0.5*TexCoord.x+0.5)*Resolution.x+(0.5*TexCoord.y+0.5)*Resolution.x*Resolution.y)

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

vec3 CosWeightedRandomHemisphereDirection( const vec3 n ) {
  vec2 r = rand2();
	vec3  uu = normalize( cross( n, vec3(0.0,1.0,1.0) ) );
	vec3  vv = cross( uu, n );
	float ra = sqrt(r.y);
	float rx = ra*cos(6.2831*r.x); 
	float ry = ra*sin(6.2831*r.x);
	float rz = sqrt( 1.0-r.y );
	vec3  rr = vec3( rx*uu + ry*vv + rz*n );
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
	float emission = (val.g & 0xFFFFu)/50.;
	float roughness = float(val.r >> 24)/255.;
	return Material(color, roughness, emission);
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

GridHit RaySceneIntersection(Ray ray, vec3 gridPos, float gridScale, int limit){
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
	while(last_voxel != curr_voxel && iter++ < limit) {
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
	if (iter == limit+1) noHit.dist = 0.;
	return noHit;
}

vec3 Trace(Ray ray, GridHit firstHit){
	vec3 rayColor = vec3(1.);
	vec3 incomingLight = vec3(0.);

	int limit = int(MapSize.x + MapSize.y + MapSize.z);
	for (int i=0; i < MAX_BOUNCES; i++){
		GridHit hitInfo;
		if (i == 0) hitInfo = firstHit;
		else hitInfo = RaySceneIntersection(ray, vec3(0.), 1., limit);

		if (!hitInfo.hit){
			if (hitInfo.dist < 0.)
				return incomingLight + rayColor * EnvironmentColor;
			return vec3(0.);
		}

		if (i != 0) {
			rayColor *= hitInfo.mat.color;
			incomingLight += rayColor * hitInfo.mat.emission;
		}

		ray.origin += ray.dir*hitInfo.dist + hitInfo.normal*EPSILON;
		
		vec3 diffuseDir = CosWeightedRandomHemisphereDirection(hitInfo.normal);

		vec3 specularDir = reflect(ray.dir, vec3(hitInfo.normal));

		ray.dir = normalize(mix(specularDir, diffuseDir, hitInfo.mat.roughness));
		ray.inverse_dir = 1.0/ray.dir;

		limit = int(pow(limit, max(0.87, 1./(i+1))));
	}

	return incomingLight;
}

float RaySphereIntersection(Ray ray, vec3 pos, float radius) {
	vec3 origin = ray.origin - pos;

	float a = dot(ray.dir, ray.dir);
	float b = 2*dot(origin, ray.dir);
	float c = dot(origin, origin) - radius*radius;

	float discriminant = b*b - 4*a*c;

	if (discriminant < 0) return -1.;

	return (-b - sqrt(discriminant))/(2*a);
}

vec2 WorldToLastScreenCoord(vec3 p){
	vec3 dir = normalize(p - LastCamPosition);

	vec3 localNearPlane = (transpose(LastCamRotation) * vec4(dir, 0.)).xyz;
	vec2 texCoord = (localNearPlane.xy/localNearPlane.z*1.5)/vec2(float(Resolution.x)/float(Resolution.y), 1.);

	return texCoord;
}

void main()
{
	// if (TexCoord.y > 0.98){
	// 	FragColor = vec3(0.0);
	// 	if (TexCoord.x*0.5 + 0.5 < float(AccumulatedFramesCount)/1000.) FragColor = vec3(0.11, 0.63, 0.87);
	// 	return;
	// }

	INIT_RNG;

	float aspect = float(Resolution.x)/float(Resolution.y);

	// vec3 dir = normalize((CamRotation * vec4(TexCoord.x*aspect, TexCoord.y, 1.5, 1.)).xyz);
	// Ray ray = Ray(CamPosition, dir, 1.0/dir);
	// FragColor = vec3(0.);
	// for	(int i = 0; i < 1500; i++){
	// 	if (RaySphereIntersection(ray, CosWeightedRandomHemisphereDirection(normalize(vec3(1., 1., 1.))), 0.005) > 0.) FragColor = vec3(1.);
	// }
	// return;

	vec3 sumColor = vec3(0.);
	vec3 localNearPlane = vec3(TexCoord.x*aspect, TexCoord.y, 1.5);

	vec3 firstDir = normalize((CamRotation * vec4(localNearPlane, 0.)).xyz);
	Ray firstRay = Ray(CamPosition, firstDir, 1.0/firstDir);
	GridHit firstHit = RaySceneIntersection(firstRay, vec3(0.), 1., int(MapSize.x + MapSize.y + MapSize.z));

	for (int s = 0; s < SAMPLES; s++) {
		vec3 offset =  vec3(2.*rand2()-1., 0.)/Resolution.y; // for anti aliasing

		sumColor += Trace(firstRay, firstHit);
	}

	vec3 color = sumColor/SAMPLES;

	// spatiotemporal denoisification

	vec3 hitPos = CamPosition + firstHit.dist * firstRay.dir;
	vec2 prevTexCoord = WorldToLastScreenCoord(hitPos);
	ivec2 prevPixel = ivec2((prevTexCoord*0.5+0.5)*Resolution);

	vec3 normalPlane = vec3(1.) - abs(firstHit.normal);

	vec3[] offsets = vec3[](
		vec3(0.),
		vec3(1., 0., 0.),
		vec3(-1., 0., 0.),
		vec3(0., 1., 0.),
		vec3(0., -1., 0.),
		vec3(0., 0., 1.),
		vec3(0., 0., -1.)
	);

	float minDiff = 1000.;
	vec2 bestCoord = prevTexCoord;
	ivec2 bestPixel = prevPixel;

	for (int i = 0; i < offsets.length(); i++){
		if (normalPlane * offsets[i] != offsets[i]) continue;

		vec3 currPos = hitPos + offsets[i]*0.05;
		vec2 currCoord = WorldToLastScreenCoord(currPos);
		ivec2 currPixel = ivec2((currCoord*0.5+0.5)*Resolution);
		vec3 actualPos = LastCamPosition + normalize(currPos - LastCamPosition) * texelFetch(LastDepthTex, currPixel, 0).r;
		float dist = distance(hitPos, actualPos);
		ivec3 normal = texture(LastNormalTex, currCoord*0.5+0.5, 0).rgb;

		//float history = texelFetch(HistoryTex, currPixel, 0).r;

		if (dist < minDiff && normal == firstHit.normal){
			minDiff = dist;
			bestCoord = currCoord;
			bestPixel = currPixel;
		}
	}

	float historyScale = 1.;
	if (LastCamPosition != CamPosition) historyScale = pow(firstHit.mat.roughness, 0.15);

	if (minDiff > 0.06) historyScale = 0.;

	FragHistory = texelFetch(HistoryTex, bestPixel, 0).r * historyScale + 1.;
	FragDepth = firstHit.dist;
	FragNormal = firstHit.normal;

	if (firstHit.hit) FragAlbedo = firstHit.mat.color;
	else FragAlbedo = vec3(1.);

	FragEmission = firstHit.mat.emission;

	FragColor = mix(texture(LastFrameTex, bestCoord*0.5+0.5).rgb, color, 1.0/(pow(FragHistory, 0.95)));
	return;
}