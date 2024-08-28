#ifndef BRICK_H
#define BRICK_H

#define OGT_VOX_IMPLEMENTATION
#define BRICK_SIZE 8

#include <glad/glad.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <io.h>
#include "camera.h"
#include "ogt_vox.h"

// parse magicavoxel file
const ogt_vox_scene* readScene(const char* filePath) {
	int err;
	FILE* fp;

	if ((err = fopen_s(&fp, filePath, "rb")) != 0) {
		std::cerr << "cannot open file " << filePath << std::endl;
		return nullptr;
	}

	uint32_t buffer_size = _filelength(_fileno(fp));
	uint8_t* buffer = new uint8_t[buffer_size];
	fread(buffer, buffer_size, 1, fp);
	fclose(fp);

	const ogt_vox_scene* scene = ogt_vox_read_scene(buffer, buffer_size);
	delete[] buffer;
	return scene;
}

void encodeData(const uint8_t* voxel_data, uint32_t* data_arr, unsigned int size_x, unsigned int size_y, unsigned int size_z) {
	for (int x = 0; x < size_x; x++)
	{
		for (int z = 0; z < size_z; z++)
		{
			const int index = (x * size_z + z) * size_y/8;

			for (int i = 0; i < (size_y / 8); i++)
			{
				data_arr[index+i] =
					(voxel_data[((i * 8 + 0) * size_x + x) * size_z + z] << 0) |
					(voxel_data[((i * 8 + 1) * size_x + x) * size_z + z] << 4) |
					(voxel_data[((i * 8 + 2) * size_x + x) * size_z + z] << 8) |
					(voxel_data[((i * 8 + 3) * size_x + x) * size_z + z] << 12) |
					(voxel_data[((i * 8 + 4) * size_x + x) * size_z + z] << 16) |
					(voxel_data[((i * 8 + 5) * size_x + x) * size_z + z] << 20) |
					(voxel_data[((i * 8 + 6) * size_x + x) * size_z + z] << 24) |
					(voxel_data[((i * 8 + 7) * size_x + x) * size_z + z] << 28);
			}
		}
	}
}

class BrickMap
{
public:
	glm::vec3 env_color;
	Camera camera;
	glm::ivec3 size;
	uint32_t* data = NULL;

	BrickMap() {}

	// read brickmap from MagicaVoxel file
	BrickMap(const char* filePath) {
		const ogt_vox_scene* scene = readScene(filePath);
		if (!scene) return;

		const ogt_vox_model* model = scene->models[0];

		size.x = model->size_x;
		size.y = model->size_z;
		size.z = model->size_y;

		if (size.y % 8 != 0) {
			std::cerr << filePath << ": brickmap's height has to be a multiple of 8." << std::endl;
			ogt_vox_destroy_scene(scene);
			return;
		}

		data = new uint32_t[size.x*size.y*size.z/8];
		encodeData(model->voxel_data, data, size.x, size.y, size.z);

		env_color = glm::vec3(scene->palette.color[255].r, scene->palette.color[255].g, scene->palette.color[255].b) * scene->materials.matl[255].emit * (float)pow(10, scene->materials.matl[255].flux) / 255.0f;

		// calculate saved camera position from file
		ogt_vox_cam voxCam = scene->cameras[0];
		glm::vec3 angles(voxCam.angle[0], -voxCam.angle[1], voxCam.angle[2]);
		glm::vec3 camFront(
			cos(glm::radians(angles.x)) * sin(glm::radians(angles.y)),
			sin(glm::radians(angles.x)),
			cos(glm::radians(angles.x)) * cos(glm::radians(angles.y)));
		glm::vec3 camPos = glm::vec3(voxCam.focus[0]+(float)size.x/2.0f, voxCam.focus[2], voxCam.focus[1] + (float)size.z / 2.0f) - glm::vec3(voxCam.radius)*camFront;
		camera = Camera(camPos, {{0.0f},{1.0f},{0.0f}}, angles.y, angles.x);
	}
	
	~BrickMap() {
		delete[] data;
	}
};

struct Material
{
	uint32_t color;
	uint16_t emission;
	uint8_t roughness;

	Material(){}
	Material(uint32_t _color, uint16_t _emission, uint16_t _roughness) : color(_color), emission(_emission), roughness(_roughness) {}
};

class Brick
{
public:
	uint32_t* data = NULL;
	Material mats[16];
	int matCount = 1;

	Brick(){}

	// read brick from MagicaVoxel file
	Brick(const char* filePath) {
		const ogt_vox_scene* scene = readScene(filePath);
		if (!scene) return;

		const ogt_vox_model* model = scene->models[0];

		if (model->size_x != BRICK_SIZE || model->size_y != BRICK_SIZE || model->size_z != BRICK_SIZE) {
			std::cerr << filePath << ": model dimensions mismatch" << std::endl;
			ogt_vox_destroy_scene(scene);
			return;
		}

		int8_t pallet_to_my_mat[256] = { 0 };
		uint8_t voxel_data[BRICK_SIZE * BRICK_SIZE * BRICK_SIZE];

		std::copy(model->voxel_data, model->voxel_data + BRICK_SIZE * BRICK_SIZE * BRICK_SIZE, std::begin(voxel_data));

		// assign materials
		for (int i = 0; i < BRICK_SIZE * BRICK_SIZE * BRICK_SIZE; i++)
		{
			if (voxel_data[i] == 0) continue;

			if (pallet_to_my_mat[voxel_data[i]] != 0) // already found
				voxel_data[i] = pallet_to_my_mat[voxel_data[i]];
			else { // assign new material
				ogt_vox_rgba ogt_color = scene->palette.color[voxel_data[i]];
				ogt_vox_matl ogt_material = scene->materials.matl[voxel_data[i]];

				mats[matCount] = Material(
					(unsigned int)(ogt_color.r) << 16 | (unsigned int)(ogt_color.g) << 8 | (unsigned int)(ogt_color.b), // color
					ogt_material.emit * 100.0f * pow(10, ogt_material.flux), // emission
					ogt_material.rough * 0xFF // roughness
				);

				pallet_to_my_mat[voxel_data[i]] = matCount;
				voxel_data[i] = matCount;
				matCount++;
			}
		}

		data = new uint32_t[BRICK_SIZE * BRICK_SIZE * BRICK_SIZE / 8];
		encodeData(voxel_data, data, BRICK_SIZE, BRICK_SIZE, BRICK_SIZE);

		ogt_vox_destroy_scene(scene);
	}

	~Brick() {
		delete[] data;
	}
};

#endif