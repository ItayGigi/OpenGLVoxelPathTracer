#ifndef CAMERA_H
#define CAMERA_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <functional>
#include "mathutil.h"

// Defines several possible options for camera movement. Used as abstraction to stay away from window-system specific input methods
enum Camera_Movement {
	FORWARD,
	BACKWARD,
	LEFT,
	RIGHT,
	UP,
	DOWN
};

// Default camera values
const float YAW = -90.0f;
const float PITCH = 0.0f;
const float SPEED = 2.5f;
const float SENSITIVITY = 0.1f;
const float ZOOM = 45.0f;

const float GRAVITY = 3.0f;

// An abstract camera class that processes input and calculates the corresponding Euler Angles, Vectors and Matrices for use in OpenGL
class Camera
{
public:
	// camera Attributes
	glm::vec3 position;
	glm::vec3 front;
	glm::vec3 up;
	glm::vec3 right;
	glm::vec3 world_up;

	// euler Angles
	float yaw;
	float pitch;

	// camera options
	float jump_force = 1.2f;
	float collider_half_width = 0.05f;

	float movement_speed;
	float mouse_sensitivity;
	float y_vel = 0.0f;
	bool is_grounded = false, is_head_bump = false;
	bool no_clip = true;

	// constructor with vectors
	Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = YAW, float pitch = PITCH) : front(glm::vec3(0.0f, 0.0f, -1.0f)), movement_speed(SPEED), mouse_sensitivity(SENSITIVITY)
	{
		this->position = position;
		this->world_up = up;
		this->yaw = yaw;
		this->pitch = pitch;
		updateCameraVectors_();
	}
	// constructor with scalar values
	Camera(float pos_x, float pos_y, float pos_z, float up_x, float up_y, float up_z, float yaw, float pitch) : front(glm::vec3(0.0f, 0.0f, -1.0f)), movement_speed(SPEED), mouse_sensitivity(SENSITIVITY)
	{
		position = glm::vec3(pos_x, pos_y, pos_z);
		world_up = glm::vec3(up_x, up_y, up_z);
		yaw = yaw;
		pitch = pitch;
		updateCameraVectors_();
	}

	// returns the view matrix calculated using Euler Angles and the LookAt Matrix
	glm::mat4 GetViewMatrix()
	{
		return glm::lookAt(position, position + front, up);
	}

	void Update(float delta_time, std::function<bool(const glm::vec3)> isPositionOccupied, glm::ivec3 grid_size) {
		if (no_clip) return;

		if (!is_grounded) {
			y_vel -= GRAVITY * delta_time;
		}

		Move(glm::vec3(0.0f, glm::sign(y_vel), 0.0f), glm::abs(y_vel) * delta_time, isPositionOccupied, grid_size);

		//std::cout << y_vel << "\n";
	}

	// processes input received from any keyboard-like input system. Accepts input parameter in the form of camera defined ENUM (to abstract it from windowing systems)
	void ProcessKeyboard(Camera_Movement direction, float delta_time, std::function<bool(const glm::vec3)> isPositionOccupied, glm::ivec3 grid_size)
	{
		float move_amount = movement_speed * delta_time;

		if (direction == FORWARD)
			Move(glm::normalize(glm::cross(right, world_up)), move_amount, isPositionOccupied, grid_size);
		if (direction == BACKWARD)
			Move(-glm::normalize(glm::cross(right, world_up)), move_amount, isPositionOccupied, grid_size);
		if (direction == LEFT)
			Move(-right, move_amount, isPositionOccupied, grid_size);
		if (direction == RIGHT)
			Move(right, move_amount, isPositionOccupied, grid_size);
		if (!no_clip && direction == UP && is_grounded)
			y_vel = jump_force;

		if (!no_clip) return;

		if (direction == DOWN)
			Move(-world_up, move_amount, isPositionOccupied, grid_size);
		if (direction == UP)
			Move(world_up, move_amount, isPositionOccupied, grid_size);
	}

	void Move(glm::vec3 dir, float amount, std::function<bool(const glm::vec3)> isPositionOccupied, glm::ivec3 grid_size)
	{
		if (no_clip) {
			position += normalize(dir) * amount;
			return;
		}

		glm::vec3 new_pos = getMovePosition_(position, dir, amount, isPositionOccupied, grid_size);
		while (isPositionOccupied(new_pos)) {
			amount -= 0.01f;
			new_pos = getMovePosition_(position, dir, amount, isPositionOccupied, grid_size);
		}
		position = new_pos;

		bool now_grounded = getMovePosition_(position, glm::vec3(0., -1., 0.), 0.1f, isPositionOccupied, grid_size) == position;

		bool now_head_bump = getMovePosition_(position, glm::vec3(0., 1., 0.), 0.1f, isPositionOccupied, grid_size) == position;

		if ((!is_grounded && now_grounded) || (!is_head_bump && now_head_bump)) {
			y_vel = 0.0f;
		}
		is_grounded = now_grounded;
		is_head_bump = now_head_bump;
	}

	// processes input received from a mouse input system. Expects the offset value in both the x and y direction.
	void ProcessMouseMovement(float xoffset, float yoffset, GLboolean constrain_pitch = true)
	{
		xoffset *= mouse_sensitivity;
		yoffset *= mouse_sensitivity;

		yaw += xoffset;
		pitch += yoffset;

		// make sure that when pitch is out of bounds, screen doesn't get flipped
		if (constrain_pitch)
		{
			if (pitch > 89.0f)
				pitch = 89.0f;
			if (pitch < -89.0f)
				pitch = -89.0f;
		}

		// update front, right and up Vectors using the updated Euler angles
		updateCameraVectors_();
	}

	// processes input received from a mouse scroll-wheel event. Only requires input on the vertical wheel-axis
	void ProcessMouseScroll(float yoffset)
	{
		movement_speed += 0.1 * movement_speed * yoffset;
	}

	glm::quat GetRotation() {
		return glm::quat(glm::vec3(glm::radians(-pitch), glm::radians(yaw), 0));
	}

	glm::vec2 WorldToScreen(const glm::vec3 world_pos, int width, int height) {
		glm::vec3 local = glm::normalize(glm::transpose(glm::mat4_cast(GetRotation())) * glm::vec4(world_pos - position, 0.0f));

		float aspect = float(width) / height;

		return glm::vec2(local.x / aspect, local.y) / glm::max(local.z, 0.00001f) * 1.5f;
	}

private:
	// calculates the front vector from the Camera's (updated) Euler Angles
	void updateCameraVectors_()
	{
		// calculate the new front vector
		//glm::vec3 front;
		front.z = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
		front.y = sin(glm::radians(pitch));
		front.x = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
		front = glm::normalize(front);
		// also re-calculate the right and up vector
		right = glm::normalize(glm::cross(-front, world_up));  // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
		up = glm::normalize(glm::cross(right, front));
	}

	glm::vec3 getMovePosition_(glm::vec3 pos, glm::vec3 dir, float amount, std::function<bool(const glm::vec3)> isPositionOccupied, glm::ivec3 grid_size) {
		dir = glm::normalize(dir);

		glm::ivec3 offsets[] = {
			glm::ivec3(1,  1,  1),
			glm::ivec3(1,  1, -1),
			glm::ivec3(1, -1,  1),
			glm::ivec3(1, -1, -1),
			glm::ivec3(-1,  1,  1),
			glm::ivec3(-1,  1, -1),
			glm::ivec3(-1, -1,  1),
			glm::ivec3(-1, -1, -1)
		};

		glm::ivec3 sides[] = {
			glm::ivec3(1,  0,  0),
			glm::ivec3(0,  1,  0),
			glm::ivec3(0,  0,  1),
			glm::ivec3(-1,  0,  0),
			glm::ivec3(0, -1,  0),
			glm::ivec3(0,  0, -1),
		};

		glm::vec3 wall_offset(0.0f);

		while (dir != glm::vec3(0.0f) && amount > 0.0f)
		{
			util::RayHit min_hit = { false, 1000.0f, glm::ivec3(0) };
			glm::ivec3 hit_normal;
			glm::ivec3 move_sign = glm::sign(dir);

			for (glm::ivec3 side : sides) {
				if (move_sign * abs(side) != side) continue;

				for (glm::ivec3 offset : offsets)
				{
					if (offset * abs(side) != side) continue;

					offset = glm::mix(offset, side, 0.8);

					glm::vec3 origin = pos + glm::vec3(offset) * collider_half_width;
					util::RayHit hit = util::rayCast(origin, dir, isPositionOccupied, 0.125f, grid_size, amount);
					if (hit.hit && hit.dist < min_hit.dist) {
						min_hit = hit;
						hit_normal = -side;
					}
				}
			}

			if (min_hit.hit && min_hit.dist >= 0.0f) {
				pos += dir * min_hit.dist + glm::vec3(hit_normal) * 0.0000f;

				amount -= min_hit.dist;
				dir *= glm::ivec3(1) - abs(min_hit.normal);
				amount *= glm::length(dir);
			}
			else {
				pos += dir * amount;
				amount = 0.0f;
			}
		}

		return pos;
	}
};
#endif