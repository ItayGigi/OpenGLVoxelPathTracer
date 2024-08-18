#ifndef MATHUTIL_H
#define MATHUTIL_H

#include <glm/glm.hpp>
#include <functional>

namespace util {
	struct SlabIntersection {
		bool hit;
		float tmin, tmax;
		glm::bvec3 mask;
	};

	SlabIntersection raySlabIntersection(glm::vec3 origin, glm::vec3 dir, glm::vec3 aabbMin, glm::vec3 aabbMax) {
		glm::vec3 tbot = (aabbMin - origin) / dir;
		glm::vec3 ttop = (aabbMax - origin) / dir;

		glm::vec3 dmin = min(ttop, tbot);
		glm::vec3 dmax = max(ttop, tbot);

		float tmin = glm::max(glm::max(dmin.x, dmin.y), dmin.z);
		float tmax = glm::min(glm::min(dmax.x, dmax.y), dmax.z);

		glm::bvec3 mask = equal(dmin, glm::vec3(tmin));

		return { tmax > glm::max(tmin, 0.0f), tmin, tmax, mask };
	}

	struct RayHit {
		bool hit;
		float dist;
		glm::ivec3 normal;
	};


	RayHit rayCast(glm::vec3 origin, glm::vec3 dir, std::function<bool(glm::vec3)> isPositionOccupied, float voxelSize, glm::ivec3 gridSize, float limit) {
		SlabIntersection boundHit = raySlabIntersection(origin, dir, glm::vec3(0.0f), glm::vec3(gridSize) * voxelSize);
		if (!boundHit.hit) return { false, INFINITY, glm::ivec3(0) };

		float tMin = boundHit.tmin;
		float tMax = boundHit.tmax;

		glm::vec3 ray_start = origin + dir * tMin;
		if (tMin < 0.) ray_start = origin;
		glm::vec3 ray_end = origin + dir * tMax;

		glm::ivec3 curr_voxel = glm::max(glm::min(glm::ivec3((ray_start) / voxelSize), gridSize - glm::ivec3(1)), glm::ivec3(0));
		glm::ivec3 last_voxel = glm::max(glm::min(glm::ivec3((ray_end) / voxelSize), gridSize - glm::ivec3(1)), glm::ivec3(0));

		glm::ivec3 step = glm::ivec3(glm::sign(dir));

		glm::vec3 t_next = (glm::vec3(curr_voxel + glm::max(step, glm::ivec3(0))) * voxelSize - ray_start) / dir;
		glm::vec3 t_delta = voxelSize / abs(dir);

		float dist = 0.;
		glm::bvec3 mask = boundHit.mask;

		int iter = 0;
		while (last_voxel != curr_voxel && iter++ < gridSize.x + gridSize.y + gridSize.z && dist < limit) {
			if (isPositionOccupied(glm::vec3(curr_voxel) * voxelSize))
				return { true, dist, -glm::ivec3(mask) * step };

			mask = glm::bvec3(t_next.x < t_next.y && t_next.x < t_next.z, t_next.y < t_next.x && t_next.y < t_next.z, t_next.z < t_next.x && t_next.z < t_next.y);

			dist += glm::min(glm::min(t_next.x, t_next.y), t_next.z);
			t_next += glm::vec3(mask) * t_delta;
			curr_voxel += glm::ivec3(mask) * step;
		}

		if (isPositionOccupied(glm::vec3(curr_voxel) * voxelSize) && dist < limit)
			return { true, dist, -glm::ivec3(mask) * step };

		return { false, INFINITY, glm::ivec3(0) };
	}
}

#endif
