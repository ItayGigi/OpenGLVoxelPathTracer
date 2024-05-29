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
#include "ogt_vox.h"

struct Material
{
	uint32_t color;
};

class Brick
{
public:
	uint32_t data[BRICK_SIZE * BRICK_SIZE * BRICK_SIZE / 8];
	Material mats[16];
	int matCount = 1;

	// read brick from MagicaVoxel file
	Brick(const char* filePath) {
		int err;
		FILE* fp;

		if ((err = fopen_s(&fp, filePath, "rb")) != 0) {
			std::cerr << "cannot open file " << filePath << std::endl;
			return;
		}

		uint32_t buffer_size = _filelength(_fileno(fp));
		uint8_t* buffer = new uint8_t[buffer_size];
		fread(buffer, buffer_size, 1, fp);
		fclose(fp);

		const ogt_vox_scene* scene = ogt_vox_read_scene(buffer, buffer_size);
		// the buffer can be safely deleted once the scene is instantiated.
		delete[] buffer;

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
				ogt_vox_rgba color = scene->palette.color[voxel_data[i]];
				mats[matCount] = { (unsigned int)(color.r) << 24 | (unsigned int)(color.g) << 16 | (unsigned int)(color.b) << 8 | (unsigned int)(color.a) };
				pallet_to_my_mat[voxel_data[i]] = matCount;
				voxel_data[i] = matCount;

				matCount++;
			}
		}

		// encode data
		for (int x = 0; x < BRICK_SIZE; x++)
		{
			for (int y = 0; y < BRICK_SIZE; y++)
			{
				for (int i = 0; i < (BRICK_SIZE / 8); i++)
				{
					const int index = (x * BRICK_SIZE + y) * BRICK_SIZE;

					data[index / 8] =
						(voxel_data[index + i * 8 + 0] << 0) |
						(voxel_data[index + i * 8 + 1] << 4) |
						(voxel_data[index + i * 8 + 2] << 8) |
						(voxel_data[index + i * 8 + 3] << 12) |
						(voxel_data[index + i * 8 + 4] << 16) |
						(voxel_data[index + i * 8 + 5] << 20) |
						(voxel_data[index + i * 8 + 6] << 24) |
						(voxel_data[index + i * 8 + 7] << 28);
				}
			}
		}

		ogt_vox_destroy_scene(scene);
	}
};

#endif