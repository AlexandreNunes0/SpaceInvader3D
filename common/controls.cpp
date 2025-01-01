#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "controls.hpp"

extern GLFWwindow *window;

glm::mat4 ViewMatrix;
glm::mat4 ProjectionMatrix;

glm::vec3 position = glm::vec3(0, 0, 5); // Default player position
float horizontalAngle = 0.0f;
float verticalAngle = 0.0f;
float initialFoV = 45.0f;

float speed = 10.0f;
float mouseSpeed = 0.005f;

int cameraMode = 1; // Default to First Person Camera

// Camera offset for Mode 1 (Player-Focused View)
glm::vec3 cameraOffset = glm::vec3(0.0f, 2.0f, 5.0f); // Fixed offset behind and slightly above the player model

glm::mat4 getViewMatrix()
{
    return ViewMatrix;
}

glm::mat4 getProjectionMatrix()
{
    return ProjectionMatrix;
}

void computeMatricesFromInput(glm::vec3 &playerPosition)
{

    static double lastTime = glfwGetTime();
    double currentTime = glfwGetTime();
    float deltaTime = float(currentTime - lastTime);

    // Get mouse position and handle movement (same as before)
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    // Update the mouse position for first-person and static modes
    if (cameraMode == 3)
    {                                                // Free camera mode: Mouse moves the camera
        glfwSetCursorPos(window, 1024 / 2, 768 / 2); // Reset mouse to center

        horizontalAngle += mouseSpeed * float(1024 / 2 - xpos);
        verticalAngle += mouseSpeed * float(768 / 2 - ypos);

        // Hide the cursor in free camera mode but allow mouse movement
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    glm::vec3 direction(
        cos(verticalAngle) * sin(horizontalAngle),
        sin(verticalAngle),
        cos(verticalAngle) * cos(horizontalAngle));

    glm::vec3 right = glm::vec3(
        sin(horizontalAngle - 3.14f / 2.0f),
        0,
        cos(horizontalAngle - 3.14f / 2.0f));

    glm::vec3 up = glm::cross(right, direction);

    // Camera movement (unchanged)
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
    {
        position += direction * deltaTime * speed;
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
    {
        position -= direction * deltaTime * speed;
    }
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
    {
        position += right * deltaTime * speed;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
    {
        position -= right * deltaTime * speed;
    }

    // Check for camera mode switch keys
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
    {
        cameraMode = 1; // Player-Focused View
    }
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
    {
        cameraMode = 2; // Static Position View
    }
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)
    {
        cameraMode = 3; // Free Camera Mode
    }

    // Set projection matrix
    float FoV = initialFoV;
    ProjectionMatrix = glm::perspective(glm::radians(FoV), 4.0f / 3.0f, 0.1f, 100.0f);

    // Handle each camera mode
    if (cameraMode == 1)
    { // Player-Focused View (Camera follows the player)
        // Camera position is slightly behind and above the player
        glm::vec3 cameraOffset = glm::vec3(0.0f, -10.0f, 5.0f); // Camera is behind and slightly above the player
        position = playerPosition + cameraOffset;               // Set the camera position relative to player

        // Camera will look slightly upwards from the player's position while still seeing the player model
        // Adjusting the look-at target to be slightly higher, so it looks from behind upwards
        glm::vec3 lookAtTarget = playerPosition + glm::vec3(0.0f, 1.5f, 0.0f); // A little higher than the player's center

        // Setting the view matrix to look at the adjusted target
        ViewMatrix = glm::lookAt(position, lookAtTarget, glm::vec3(0.0f, 1.0f, 0.0f));

        // Disable mouse cursor in Mode 1
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    else if (cameraMode == 2)
    {                                             // Static Position View (Top-Down view of the game board)
        position = glm::vec3(0.0f, 20.0f, 90.0f); // Fixed position

        // Looking directly down towards the center of the board
        ViewMatrix = glm::lookAt(position, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));

        // Disable mouse cursor in Mode 2
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
    else
    { // Free Camera Mode (Unchanged)
        ViewMatrix = glm::lookAt(position, position + direction, up);

        // Hide the cursor and allow mouse movement in Mode 3
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    lastTime = currentTime;
}
