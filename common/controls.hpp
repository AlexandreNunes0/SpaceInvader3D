#ifndef CONTROLS_HPP
#define CONTROLS_HPP

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Forward declarations
extern GLFWwindow* window;
extern glm::mat4 ViewMatrix;
extern glm::mat4 ProjectionMatrix;

extern glm::vec3 position;  // Camera position
extern float horizontalAngle;
extern float verticalAngle;
extern float initialFoV;
extern float speed;
extern float mouseSpeed;

extern int cameraMode;  // 1 = Top-Down, 2 = Player-Focused, 3 = Free Camera

// Function declarations
glm::mat4 getViewMatrix();
glm::mat4 getProjectionMatrix();
void computeMatricesFromInput(glm::vec3& playerPosition);

#endif
