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


// An abstract camera class that processes input and calculates the corresponding Euler Angles, Vectors and Matrices for use in OpenGL
class Camera
{
public:
    // camera Attributes
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;
    // euler Angles
    float Yaw;
    float Pitch;
    // camera options
    float MovementSpeed;
    float MouseSensitivity;

    // constructor with vectors
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = YAW, float pitch = PITCH) : Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY)
    {
        Position = position;
        WorldUp = up;
        Yaw = yaw;
        Pitch = pitch;
        updateCameraVectors();
    }
    // constructor with scalar values
    Camera(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch) : Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY)
    {
        Position = glm::vec3(posX, posY, posZ);
        WorldUp = glm::vec3(upX, upY, upZ);
        Yaw = yaw;
        Pitch = pitch;
        updateCameraVectors();
    }

    // returns the view matrix calculated using Euler Angles and the LookAt Matrix
    glm::mat4 GetViewMatrix()
    {
        return glm::lookAt(Position, Position + Front, Up);
    }

    // processes input received from any keyboard-like input system. Accepts input parameter in the form of camera defined ENUM (to abstract it from windowing systems)
    void ProcessKeyboard(Camera_Movement direction, float deltaTime, std::function<bool(const glm::vec3)> isPositionOccupied, glm::ivec3 gridSize)
    {
        float moveAmount = MovementSpeed * deltaTime;
        glm::vec3 movement(0.0f);

        if (direction == FORWARD)
            movement = glm::normalize(glm::cross(Right, WorldUp));
        if (direction == BACKWARD)
            movement = -glm::normalize(glm::cross(Right, WorldUp));
        if (direction == LEFT)
            movement = -Right;
        if (direction == RIGHT)
            movement = Right;
        if (direction == UP)
            movement = WorldUp;
        if (direction == DOWN)
            movement = -WorldUp;

        glm::vec3 offsets[] = {
            {{1.0f}, {0.0f}, {0.0f}},
            {{0.0f}, {1.0f}, {0.0f}},
            {{0.0f}, {0.0f}, {1.0f}} };

        while (movement != glm::vec3(0.0f, 0.0f, 0.0f) && moveAmount > 0.0f)
        {
            util::RayHit minHit = { false, 1000.0f, glm::ivec3(0) };

            for (glm::vec3 offset : offsets)
            {
                if (movement * offset == glm::vec3(0.0f, 0.0f, 0.0f)) continue;

                util::RayHit hit = util::rayCast(Position + glm::sign(movement) * offset * 0.1f, movement, isPositionOccupied, 0.125f, gridSize, moveAmount);
                if (hit.hit && hit.dist < minHit.dist) {
                    minHit = hit;
                }
            }

            if (minHit.hit && minHit.dist >= 0.0f) {
                Position += movement * minHit.dist;

                moveAmount -= minHit.dist;
                movement *= glm::ivec3(1) - abs(minHit.normal);
                moveAmount *= glm::length(movement);
            }
            else {
                Position += movement * moveAmount;
                moveAmount = 0.0f;
            }
        }
    }

    // processes input received from a mouse input system. Expects the offset value in both the x and y direction.
    void ProcessMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch = true)
    {
        xoffset *= MouseSensitivity;
        yoffset *= MouseSensitivity;

        Yaw += xoffset;
        Pitch += yoffset;

        // make sure that when pitch is out of bounds, screen doesn't get flipped
        if (constrainPitch)
        {
            if (Pitch > 89.0f)
                Pitch = 89.0f;
            if (Pitch < -89.0f)
                Pitch = -89.0f;
        }

        // update Front, Right and Up Vectors using the updated Euler angles
        updateCameraVectors();
    }

    // processes input received from a mouse scroll-wheel event. Only requires input on the vertical wheel-axis
    void ProcessMouseScroll(float yoffset)
    {
        MovementSpeed += 0.1 * MovementSpeed * yoffset;
    }

    glm::quat GetRotation() {
        return glm::quat(glm::vec3(glm::radians(-Pitch), glm::radians(Yaw), 0));
    }

private:
    // calculates the front vector from the Camera's (updated) Euler Angles
    void updateCameraVectors()
    {
        // calculate the new Front vector
        glm::vec3 front;
        front.z = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        front.y = sin(glm::radians(Pitch));
        front.x = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        Front = glm::normalize(front);
        // also re-calculate the Right and Up vector
        Right = glm::normalize(glm::cross(-Front, WorldUp));  // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
        Up = glm::normalize(glm::cross(Right, Front));
    }
};
#endif