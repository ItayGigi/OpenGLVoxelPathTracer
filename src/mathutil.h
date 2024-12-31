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
}

#endif
