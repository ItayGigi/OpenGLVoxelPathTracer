#ifndef SCENE_H
#define SCENE_H

#include "brick.h"
#include "mathutil.h"

#include <vector>

class Scene {
public:
	std::unique_ptr<BrickMap> brick_map;
	std::vector<std::unique_ptr<Brick>> bricks;
	std::vector<uint32_t> mats_data;
	float sun_strength;

	bool LoadFromFile(const std::string path) {
		std::ifstream scene_file(path);

		if (!scene_file) {
			std::cout << "Scene file \'" << path << "\' not found.\n";
			return false;
		}

		std::string file_folder = path.substr(0, std::min(path.find_last_of('/'), path.find_last_of('\\'))) + "/";

		// map
		std::string brickmap_path;
		scene_file >> brickmap_path;

		BrickMap* new_map = new BrickMap((file_folder + brickmap_path).c_str());
		if (new_map->env_color == glm::vec3(-1.f)) { // failed to load brickmap
			delete new_map;
			return false;
		}

		std::string sky_setting;
		scene_file >> sky_setting;

		sun_strength = 0.0f;
		if (sky_setting == "sky") {
			new_map->env_color = glm::vec3(-1);
			scene_file >> sun_strength;
			if (scene_file.fail()) {
				std::cout << "Expected sun strength after sky keyword.\n";
				delete new_map;
				return false;
			}
		}
		else if (sky_setting != "color") {
			std::cout << "Input sky setting \'" << sky_setting << "\' is invalid. Expected \'color\' or \'sky\'.\n";
			delete new_map;
			return false;
		}

		std::vector<std::string> brick_paths;
		std::string next_brick_path;
		while (scene_file >> next_brick_path)
			brick_paths.push_back(next_brick_path);

		std::vector<Brick*> new_bricks;

		for (int i = 0; i < brick_paths.size(); i++)
		{
			new_bricks.push_back(new Brick((file_folder + brick_paths[i]).c_str()));

			Brick* brick = new_bricks.back();

			if (brick->data.empty()) { // failed to load brick
				delete new_map;
				for (Brick* brick : new_bricks) delete brick;
				return false;
			}
		}

		brick_map = std::unique_ptr<BrickMap>(new_map);
		bricks.clear();
		mats_data.resize(brick_paths.size() * 16 * 2);

		for (int i = 0; i < brick_paths.size(); i++)
		{
			bricks.push_back(std::unique_ptr<Brick>(new_bricks[i]));
			Brick* brick = bricks.back().get();

			for (int j = 1; j < brick->mats.size(); j++) // load all brick's materials
			{
				mats_data[(i * 16 + j) * 2 + 0] = brick->mats[j].color | (brick->mats[j].roughness << 24);
				mats_data[(i * 16 + j) * 2 + 1] = brick->mats[j].emission;
			}
		}

		return true;
	}

	void LoadEmpty() {
		brick_map = std::unique_ptr<BrickMap>(new BrickMap());

		mats_data.clear();

		bricks.clear();
	}

	bool IsPositionOccupied(const glm::vec3 pos) {
		if (pos.x < 0.0f || pos.x >= brick_map->size.x || pos.y < 0.0f || pos.y >= brick_map->size.y || pos.z < 0.0f || pos.z >= brick_map->size.z)
			return false;

		unsigned int brick_ID = brick_map->getVoxel(pos.x, pos.y, pos.z);

		if (brick_ID == 0) return false; // air

		glm::ivec3 in_brick_pos = glm::ivec3(pos * float(config::BrickSize)) % config::BrickSize;
		unsigned int voxel_mat = bricks[brick_ID - 1]->getVoxel(in_brick_pos.x, in_brick_pos.y, in_brick_pos.z);

		return voxel_mat != 0;
	}

	util::RayHit CastRay(glm::vec3 origin, glm::vec3 dir, float limit) {
		glm::ivec3 grid_size = brick_map->size * config::BrickSize;
		float voxel_size = 1. / config::BrickSize;

		util::SlabIntersection bound_hit = util::raySlabIntersection(origin, dir, glm::vec3(0.0f), glm::vec3(brick_map->size));
		if (!bound_hit.did_hit) return { false, INFINITY, glm::ivec3(0) };

		float tMin = bound_hit.tmin;
		float tMax = bound_hit.tmax;

		glm::vec3 ray_start = origin + dir * tMin;
		if (tMin < 0.) ray_start = origin;
		glm::vec3 ray_end = origin + dir * tMax;

		glm::ivec3 curr_voxel = glm::max(glm::min(glm::ivec3((ray_start) / voxel_size), grid_size - glm::ivec3(1)), glm::ivec3(0));
		glm::ivec3 last_voxel = glm::max(glm::min(glm::ivec3((ray_end) / voxel_size), grid_size - glm::ivec3(1)), glm::ivec3(0));

		glm::ivec3 step = glm::ivec3(glm::sign(dir));

		glm::vec3 t_next = (glm::vec3(curr_voxel + glm::max(step, glm::ivec3(0))) * voxel_size - ray_start) / dir;
		glm::vec3 t_delta = voxel_size / abs(dir);

		float dist = 0.;
		glm::bvec3 mask = bound_hit.mask;

		int iter = 0;
		while (last_voxel != curr_voxel && iter++ < grid_size.x + grid_size.y + grid_size.z && dist + glm::max(tMin, 0.0f) < limit) {
			if (IsPositionOccupied(glm::vec3(curr_voxel) * voxel_size + glm::vec3(0.001)))
				return { true, dist + glm::max(tMin, 0.0f), -glm::ivec3(mask) * step };

			mask = glm::bvec3(
				t_next.x < t_next.y && t_next.x < t_next.z,
				t_next.y < t_next.x && t_next.y < t_next.z,
				t_next.z < t_next.x && t_next.z < t_next.y);

			dist = glm::min(glm::min(t_next.x, t_next.y), t_next.z);

			t_next += glm::vec3(mask) * t_delta;
			curr_voxel += glm::ivec3(mask) * step;
		}

		if (IsPositionOccupied(glm::vec3(curr_voxel) * voxel_size) && dist + glm::max(tMin, 0.0f) < limit)
			return { true, dist + glm::max(tMin, 0.0f), -glm::ivec3(mask) * step };

		return { false, INFINITY, glm::ivec3(0) };
	}
};

#endif