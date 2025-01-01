#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <iostream>
#include <map>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
GLFWwindow *window;
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;

#include "common/shader.hpp"
#include "common/controls.hpp"
#include "common/vboindexer.hpp"
#include "common/texture.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define DEBUG true
#define DEBUG_PRINT(msg)               \
    if (DEBUG)                         \
    {                                  \
        std::cout << msg << std::endl; \
    }

extern GLFWwindow *window;
extern glm::mat4 ViewMatrix;
extern glm::mat4 ProjectionMatrix;

////////////////////////////////////////////////////
//              Global Variables
//
float alienSpeed = 0.02f;         // Speed of alien movement
float LEFTBOUNDARY = -50.0f;      // Left boundary
float RIGHTBOUNDARY = 50.0f;      // Right boundary
bool alienMovingRight = true;     // Current direction of aliens
float alienDropDistance = 0.5f;   // Distance to move down when hitting a boundary
const float PLAYER_SPEED = 20.0f; // Player speed
int nextObjectId = 0;             // Global counter for unique IDs
double lastShotTime = 0.0f;
const float SHOT_COOLDOWN = 0.5f;    // 0.5 second cooldown
bool mothershipAlive = true;         // Track whether the mothership is alive or not
int mothershipHealth;                // Mothership health, can be adjusted
bool mothershipMovingRight = true;   // Mothership direction
const float mothershipSpeed = 0.05f; // Mothership movement speed

////////////////////////////////////////////////////
//              Structures
//
// Cache structure to hold parsed OBJ data
struct ObjCache
{
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec3> normals;
    std::vector<tinyobj::material_t> materials;
    std::vector<std::string> textures;
    std::vector<GLuint> textureIDs;
};

// Structure for the game objects
struct GameObject
{
    std::string objFile;
    std::string mtlFile;
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec3> normals;
    std::vector<tinyobj::material_t> materials;
    std::vector<std::string> textures;
    std::vector<GLuint> textureIDs;
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f); // Default position
    glm::mat4 modelMatrix = glm::mat4(1.0f);          // Default identity matrix
    GLuint vertexArrayID = 0;
    GLuint vertexBuffer = 0;
    GLuint uvBuffer = 0;
    GLuint normalBuffer = 0;
    int id = -1; // Unique identifier for each object
    std::string type;

    // Constructor to initialize the GameObject
    GameObject()
        : position(glm::vec3(0.0f, 0.0f, 0.0f)),
          modelMatrix(glm::mat4(1.0f)),
          vertexArrayID(0), vertexBuffer(0), uvBuffer(0), normalBuffer(0), id(-1)
    {
    }
};

struct Laser
{
    glm::vec3 direction = glm::vec3(0.0f, 1.0f, 0.0f); // Moving upwards
    float speed = 10.0f;                               // Speed of laser
    bool active = false;                               // Whether the laser is active or not

    GameObject obj; // The laser is a game object, too
};

std::map<std::string, ObjCache> objCache;
std::vector<Laser> lasers;

////////////////////////////////////////////////////
//  Functions  for loading files
//
//
// Function to load textures and cache them
GLuint loadTexture(const std::string &texturePath)
{

    int width, height, channels;
    DEBUG_PRINT("Attempting to load texture from: " << texturePath);
    unsigned char *data = stbi_load(texturePath.c_str(), &width, &height, &channels, 0);
    if (!data)
    {
        DEBUG_PRINT("Failed to load texture: " << texturePath);
        DEBUG_PRINT("stbi_error: " << stbi_failure_reason());
        return 0;
    }

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    if (channels == 3)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    }
    else if (channels == 4)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    }

    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);

    DEBUG_PRINT("Successfully loaded texture: " << texturePath);
    return textureID;
}

// Function to load the OBJ file and materials, and store the data
bool OBJloadingfunction(const char *objpath, const char *mtlpath,
                        std::vector<glm::vec3> &out_vertices,
                        std::vector<glm::vec2> &out_uvs,
                        std::vector<glm::vec3> &out_normals,
                        std::vector<tinyobj::material_t> &out_materials,
                        std::vector<std::string> &out_textures,
                        std::vector<GLuint> &out_textureIDs)
{

    // Check if the OBJ file is already cached
    auto it = objCache.find(objpath);
    if (it != objCache.end())
    {
        DEBUG_PRINT("Object Data Already Exists -- Using cached data!");
        // Use cached data
        const ObjCache &cache = it->second;
        out_vertices = cache.vertices;
        out_uvs = cache.uvs;
        out_normals = cache.normals;
        out_materials = cache.materials;
        out_textures = cache.textures;
        out_textureIDs = cache.textureIDs;
        return true;
    }

    // Proceed with loading the OBJ file if not cached
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    DEBUG_PRINT("Loading OBJ file: " << objpath);
    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, objpath, mtlpath);

    if (!warn.empty())
    {
        std::cerr << "Warning: " << warn << std::endl;
    }
    if (!err.empty())
    {
        std::cerr << "Error: " << err << std::endl;
        return false;
    }

    out_materials = materials;

    std::string mtlFolderPath = std::string(mtlpath);
    size_t lastSlash = mtlFolderPath.find_last_of('/');
    if (lastSlash != std::string::npos)
    {
        mtlFolderPath = mtlFolderPath.substr(0, lastSlash);
    }

    for (const auto &material : materials)
    {
        if (!material.diffuse_texname.empty())
        {
            std::string fullTexturePath = mtlFolderPath + "/" + material.diffuse_texname;
            out_textures.push_back(fullTexturePath);
            DEBUG_PRINT("Found texture: " << fullTexturePath);
        }
        else
        {
            out_textures.push_back("");
        }
    }

    for (const auto &texturePath : out_textures)
    {
        if (!texturePath.empty())
        {
            GLuint textureID = loadTexture(texturePath);
            out_textureIDs.push_back(textureID);
        }
        else
        {
            out_textureIDs.push_back(0);
        }
    }

    for (const auto &shape : shapes)
    {
        for (const auto &index : shape.mesh.indices)
        {
            glm::vec3 vertex;
            glm::vec2 uv;
            glm::vec3 normal;

            vertex.x = attrib.vertices[3 * index.vertex_index + 0];
            vertex.y = attrib.vertices[3 * index.vertex_index + 1];
            vertex.z = attrib.vertices[3 * index.vertex_index + 2];
            out_vertices.push_back(vertex);

            if (index.normal_index >= 0)
            {
                normal.x = attrib.normals[3 * index.normal_index + 0];
                normal.y = attrib.normals[3 * index.normal_index + 1];
                normal.z = attrib.normals[3 * index.normal_index + 2];
                out_normals.push_back(normal);
            }

            if (index.texcoord_index >= 0)
            {
                uv.x = attrib.texcoords[2 * index.texcoord_index + 0];
                uv.y = attrib.texcoords[2 * index.texcoord_index + 1];
                out_uvs.push_back(uv);
            }
        }
    }

    // Cache the loaded data
    ObjCache cache;
    cache.vertices = out_vertices;
    cache.uvs = out_uvs;
    cache.normals = out_normals;
    cache.materials = out_materials;
    cache.textures = out_textures;
    cache.textureIDs = out_textureIDs;
    objCache[objpath] = cache;

    DEBUG_PRINT("OBJ file loaded successfully!");
    return true;
}

////////////////////////////////////////////////////
//      Functions to load game objects
//
// Function to load game object data and initialize buffers
void loadGameObject(GameObject &obj)
{
    DEBUG_PRINT("Loading GameObject: " << obj.objFile);
    if (!OBJloadingfunction(obj.objFile.c_str(), obj.mtlFile.c_str(), obj.vertices, obj.uvs, obj.normals, obj.materials, obj.textures, obj.textureIDs))
    {
        DEBUG_PRINT("Failed to load GameObject: " << obj.objFile);
        return;
    }

    glGenVertexArrays(1, &obj.vertexArrayID);
    glBindVertexArray(obj.vertexArrayID);

    // Vertex Buffer
    glGenBuffers(1, &obj.vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, obj.vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, obj.vertices.size() * sizeof(glm::vec3), &obj.vertices[0], GL_STATIC_DRAW);

    // UV Buffer
    glGenBuffers(1, &obj.uvBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, obj.uvBuffer);
    glBufferData(GL_ARRAY_BUFFER, obj.uvs.size() * sizeof(glm::vec2), &obj.uvs[0], GL_STATIC_DRAW);

    // Normal Buffer
    glGenBuffers(1, &obj.normalBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, obj.normalBuffer);
    glBufferData(GL_ARRAY_BUFFER, obj.normals.size() * sizeof(glm::vec3), &obj.normals[0], GL_STATIC_DRAW);

    // Set up texture IDs
    for (GLuint textureID : obj.textureIDs)
    {
        if (textureID != 0)
        {
            glBindTexture(GL_TEXTURE_2D, textureID);
        }
    }
}

// Function to generate unique IDs for game objects
int generateUniqueID()
{
    return nextObjectId++;
}

// Function to load the player ship
void createPlayer(GameObject &playerShip)
{
    DEBUG_PRINT("Creating player...");
    playerShip.objFile = "obj/player.obj";
    playerShip.mtlFile = "obj";
    playerShip.position = glm::vec3(2.0f, -22.0f, 0.0f); // Set player lower in the scene
    playerShip.id = generateUniqueID();                  // Assign a unique ID to the player
    playerShip.type = "Player";
    loadGameObject(playerShip);
    DEBUG_PRINT("Player created with ID: " << playerShip.id);
    DEBUG_PRINT("-----------------------------------------");
}

// Function to load the mothership
void createMothership(GameObject &motherShip)
{
    DEBUG_PRINT("Creating mothership...");
    motherShip.objFile = "obj/mothership.obj";
    motherShip.mtlFile = "obj";
    motherShip.position = glm::vec3(1.0f, 12.0f, 0.0f);
    motherShip.id = generateUniqueID(); // Assign unique ID
    motherShip.type = "MotherShip";
    mothershipAlive = true;    // Ensure it's alive when the game starts
    int mothershipHealth = 10; // Mothership health, can be adjusted
    loadGameObject(motherShip);
    DEBUG_PRINT("Mothership created with ID: " << motherShip.id);
    DEBUG_PRINT("-----------------------------------------");
}

// Function to create a laser
void createLaser(Laser &laser, const glm::vec3 &startPos)
{
    laser.obj.position = startPos;
    laser.obj.objFile = "obj/laser.obj";
    laser.obj.type = "Laser";
    laser.obj.mtlFile = "obj"; // Same texture folder as other objects
    loadGameObject(laser.obj);
    laser.active = true;
}

// Function to create aliens dynamically with flexibility
void createAliens(std::vector<GameObject> &aliens_vector, int rows = 5, int cols = 4, float spacing = 5.0f, const glm::vec3 &startPos = glm::vec3(-6.0f, 4.0f, 0.0f))
{
    // Array of alien models to be assigned dynamically
    std::vector<std::string> alienModels = {"obj/alien1.obj", "obj/alien2.obj", "obj/alien3.obj"};
    DEBUG_PRINT("Creating aliens...");

    // Loop through rows and columns to create aliens dynamically
    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            GameObject alien;

            // Dynamically assign alien model based on row or any custom logic
            // Here, we cycle through the alien models based on row
            alien.objFile = alienModels[row % alienModels.size()];
            alien.mtlFile = "obj/"; // Assuming all have the same material path for now
            alien.type = "Alien";
            // Position the alien in the grid, adjust spacing and start position
            alien.position = startPos + glm::vec3(col * spacing, -row * spacing, 0.0f);

            alien.id = generateUniqueID(); // Assign unique ID to each alien

            loadGameObject(alien);
            aliens_vector.push_back(alien);
        }
    }

    DEBUG_PRINT("Created: " << aliens_vector.size() << " Aliens!");
}

// Function to initialize the game objects (Player, Mothership, Aliens)
void initializeGameObjects(std::vector<GameObject> &aliens_vector, GameObject &playerShip, GameObject &motherShip)
{
    createPlayer(playerShip);
    createMothership(motherShip);
    createAliens(aliens_vector, 6, 5, 5.0f, glm::vec3(-8.0f, 5.0f, 0.0f));
}

// Function to render any game object
void renderObject(const GameObject &obj, GLuint MatrixID, GLuint ModelMatrixID, GLuint ViewMatrixID, GLuint textureID, const glm::mat4 &ProjectionMatrix, const glm::mat4 &ViewMatrix)
{
    // Set up model matrix for translation
    glm::mat4 MVP = ProjectionMatrix * ViewMatrix * obj.modelMatrix;

    // Send the transformation matrices to the shaders
    glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);
    glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &obj.modelMatrix[0][0]);
    glUniformMatrix4fv(ViewMatrixID, 1, GL_FALSE, &ViewMatrix[0][0]);

    // Bind textures
    for (size_t i = 0; i < obj.textures.size(); i++)
    {
        GLuint texID = obj.textureIDs[i];
        if (obj.textureIDs[i] != 0)
        {
            glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(i));
            glBindTexture(GL_TEXTURE_2D, texID);
            glUniform1i(textureID, static_cast<GLuint>(i));
        }
    }

    // Bind VAO and draw
    glBindVertexArray(obj.vertexArrayID);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, obj.vertexBuffer);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);

    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, obj.uvBuffer);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);

    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, obj.normalBuffer);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);

    // Draw the object
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(obj.vertices.size()));

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
}

////////////////////////////////////////////////////////
//               Functions For Final Cleanup
//
// Function to clean up OpenGL resources for a GameObject
void cleanupGameObject(GameObject &obj)
{
    DEBUG_PRINT("Cleaning up GameObject With ID = " << obj.id << " and Type = " << obj.type);

    // Delete OpenGL buffers
    if (obj.vertexBuffer)
    {
        glDeleteBuffers(1, &obj.vertexBuffer);
        obj.vertexBuffer = 0;
    }
    if (obj.uvBuffer)
    {
        glDeleteBuffers(1, &obj.uvBuffer);
        obj.uvBuffer = 0;
    }
    if (obj.normalBuffer)
    {
        glDeleteBuffers(1, &obj.normalBuffer);
        obj.normalBuffer = 0;
    }

    // Delete VAO
    if (obj.vertexArrayID)
    {
        glDeleteVertexArrays(1, &obj.vertexArrayID);
        obj.vertexArrayID = 0;
    }

    // Clear texture IDs
    for (GLuint &textureID : obj.textureIDs)
    {
        if (textureID)
        {
            glDeleteTextures(1, &textureID);
            textureID = 0;
        }
    }
}

// General cleanup function to clear all resources
void cleanup(std::vector<GameObject> &aliens, GameObject &playerShip, GameObject &motherShip)
{

    DEBUG_PRINT("-----------------------------------------");
    // Cleanup player ship
    cleanupGameObject(playerShip);

    if (mothershipAlive)
    {
        cleanupGameObject(motherShip); // Clean up mothership only if alive
    }
    // Cleanup aliens
    for (auto &alien : aliens)
    {
        cleanupGameObject(alien);
    }
    aliens.clear();

    // Inside cleanup
    for (auto &laser : lasers)
    {
        cleanupGameObject(laser.obj);
    }
    lasers.clear();

    // Clear object cache
    objCache.clear();

    DEBUG_PRINT("Cleanup complete!");
    DEBUG_PRINT("-----------------------------------------");
}

////////////////////////////////////////////////////////
//      Functions to Handle Object Interactivity
//
// Function to handle player movement

void handlePlayerMovement(GameObject &player, float deltaTime)
{
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    {
        player.position.x -= PLAYER_SPEED * deltaTime;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    {
        player.position.x += PLAYER_SPEED * deltaTime;
    }

    // Ensure player stays within boundaries
    if (player.position.x < LEFTBOUNDARY)
    {
        player.position.x = LEFTBOUNDARY;
    }
    if (player.position.x > RIGHTBOUNDARY)
    {
        player.position.x = RIGHTBOUNDARY;
    }

    // Fire laser when Spacebar is pressed
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && (glfwGetTime() - lastShotTime) >= SHOT_COOLDOWN)
    {
        Laser newLaser;
        createLaser(newLaser, player.position + glm::vec3(0.0f, 2.0f, 0.0f)); // Position above the player
        lasers.push_back(newLaser);
        lastShotTime = glfwGetTime();
    }
}

void updateAlienPositions(std::vector<GameObject> &aliens_vector)
{
    bool hitBoundary = false;

    // Check for boundary collision
    for (auto &alien : aliens_vector)
    {
        if (alienMovingRight && alien.position.x > RIGHTBOUNDARY)
        {
            hitBoundary = true;
        }
        if (!alienMovingRight && alien.position.x < LEFTBOUNDARY)
        {
            hitBoundary = true;
        }
    }

    // If a boundary is hit, reverse direction and move down
    if (hitBoundary)
    {
        alienMovingRight = !alienMovingRight; // Reverse direction
        for (auto &alien : aliens_vector)
        {
            alien.position.y -= alienDropDistance; // Move down
        }
    }

    // Update positions based on the direction
    float direction = alienMovingRight ? 1.0f : -1.0f;
    for (auto &alien : aliens_vector)
    {
        alien.position.x += alienSpeed * direction;
    }
}

// Function to update mothership position
void updateMothershipPosition(GameObject &motherShip)
{
    // Reverse direction if it hits the screen boundaries
    if (motherShip.position.x < LEFTBOUNDARY || motherShip.position.x > RIGHTBOUNDARY)
    {
        mothershipMovingRight = !mothershipMovingRight; // Reverse direction
    }

    // Move the mothership based on its direction
    if (mothershipMovingRight)
    {
        motherShip.position.x += mothershipSpeed;
    }
    else
    {
        motherShip.position.x -= mothershipSpeed;
    }
}

// Function to update laser position
void updateLaser(Laser &laser, float deltaTime)
{
    if (!laser.active)
        return; // Skip if laser is inactive

    if (laser.active)
    {
        laser.obj.position += laser.direction * laser.speed * deltaTime;
        // Deactivate laser when it moves off-screen
        if (laser.obj.position.y > 35.0f)
        {
            laser.active = false;
        }
    }
}

// Function to render the laser
void renderLaser(Laser &laser, GLuint MatrixID, GLuint ModelMatrixID, GLuint ViewMatrixID, GLuint textureID, const glm::mat4 &ProjectionMatrix, const glm::mat4 &ViewMatrix)
{
    if (laser.active)
    {
        // Now you can modify the laser object since it's passed as a non-const reference
        laser.obj.modelMatrix = glm::translate(glm::mat4(1.0f), laser.obj.position);
        renderObject(laser.obj, MatrixID, ModelMatrixID, ViewMatrixID, textureID, ProjectionMatrix, ViewMatrix);
    }
}

bool checkLaserAlienCollision(const Laser &laser, GameObject &alien)
{
    // Simple distance check (bounding box or radius-based)
    float distance = glm::length(laser.obj.position - alien.position);

    // Assuming some threshold distance (you may adjust this depending on your alien size)
    float threshold = 2.0f;

    return distance < threshold; // Return true if collision happens
}

// Function to handle laser-alien collisions
void handleLaserAlienCollisions(std::vector<GameObject> &aliens)
{
    for (auto laserIt = lasers.begin(); laserIt != lasers.end();)
    {
        if (!laserIt->active)
        {
            // If laser is inactive, remove it from the array
            laserIt = lasers.erase(laserIt);
        }
        else
        {
            // Otherwise, check for collision with aliens
            for (auto it = aliens.begin(); it != aliens.end();)
            {
                if (checkLaserAlienCollision(*laserIt, *it))
                {
                    it = aliens.erase(it);   // Erase alien on collision
                    laserIt->active = false; // Deactivate laser
                    break;                   // Exit once laser hits an alien
                }
                else
                {
                    ++it; // Continue checking other aliens
                }
            }

            ++laserIt; // Move to next laser
        }
    }
}

bool checkLaserMothershipCollision(const Laser &laser, GameObject &motherShip)
{
    if (!mothershipAlive)
        return false; // If mothership is already dead, skip collision check

    // Check if the laser position is within range of the mothership
    float distance = glm::length(laser.obj.position - motherShip.position);

    // Define a threshold distance for collision detection (you can tweak this value)
    float threshold = 2.0f;

    return distance < threshold; // Return true if collision occurs
}

void handleLaserMothershipCollision(GameObject &mothership)
{
    for (auto laserIt = lasers.begin(); laserIt != lasers.end();)
    {
        if (!laserIt->active)
        {
            // Remove inactive lasers
            laserIt = lasers.erase(laserIt);
        }
        else
        {
            // Check for collision with mothership
            if (checkLaserMothershipCollision(*laserIt, mothership))
            {
                mothershipHealth -= 1;   // Reduce mothership health on hit
                laserIt->active = false; // Deactivate laser
                if (mothershipHealth <= 0)
                {
                    mothershipAlive = false; // Mothership is destroyed
                    DEBUG_PRINT("Mothership Destroyed!");
                    cleanupGameObject(mothership); // Clean up mothership only if alive
                }
                break; // Stop checking after a hit
            }
            else
            {
                ++laserIt; // Continue to next laser
            }
        }
    }
}

////////////////////////////////////////////////////////
//          Main Function
//
int main(void)
{
    // GLFW and GLEW initialization code
    if (!glfwInit())
    {
        DEBUG_PRINT("Failed to initialize GLFW!");
        return -1;
    }

    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(1920, 1080, "Space Invaders | Trabalho Final", NULL, NULL);
    if (window == NULL)
    {
        DEBUG_PRINT("Failed to open GLFW window!");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glewExperimental = true;
    if (glewInit() != GLEW_OK)
    {
        DEBUG_PRINT("Failed to initialize GLEW!");
        glfwTerminate();
        return -1;
    }

    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPos(window, 1920 / 2, 1080 / 2);

    glClearColor(0.4f, 0.4f, 0.4f, 0.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);

    GLuint programID = LoadShaders("TransformVertexShader.vertexshader", "TextureFragmentShader.fragmentshader");
    DEBUG_PRINT("Shaders loaded successfully!");
    DEBUG_PRINT("-----------------------------------------");

    GLuint MatrixID = glGetUniformLocation(programID, "MVP");
    GLuint ViewMatrixID = glGetUniformLocation(programID, "V");
    GLuint ModelMatrixID = glGetUniformLocation(programID, "M");
    GLuint textureID = glGetUniformLocation(programID, "textureSampler");

    std::vector<GameObject> aliens_vector;
    GameObject playerShip, motherShip;

    // Initialize all game objects (player, mothership, aliens)
    initializeGameObjects(aliens_vector, playerShip, motherShip);

    double lastTime = glfwGetTime();

    // Main render loop
    do
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(programID);

        // Calculate deltaTime for smooth movement
        double currentTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;

        computeMatricesFromInput(playerShip.position);

        glm::mat4 ProjectionMatrix = getProjectionMatrix();
        glm::mat4 ViewMatrix = getViewMatrix();

        handlePlayerMovement(playerShip, deltaTime);

        updateAlienPositions(aliens_vector);

        // Render player ship
        playerShip.modelMatrix = glm::translate(glm::mat4(1.0f), playerShip.position);
        playerShip.modelMatrix = glm::scale(playerShip.modelMatrix, glm::vec3(0.5f, 0.5f, 0.5f));
        renderObject(playerShip, MatrixID, ModelMatrixID, ViewMatrixID, textureID, ProjectionMatrix, ViewMatrix);

        // Render the mothership only if it's alive
        if (mothershipAlive)
        {
            updateMothershipPosition(motherShip);
            motherShip.modelMatrix = glm::translate(glm::mat4(1.0f), motherShip.position);
            motherShip.modelMatrix = glm::scale(motherShip.modelMatrix, glm::vec3(0.5f, 0.5f, 0.5f));
            renderObject(motherShip, MatrixID, ModelMatrixID, ViewMatrixID, textureID, ProjectionMatrix, ViewMatrix);
        }

        // Render each alien dynamically
        for (auto &alien : aliens_vector)
        {
            alien.modelMatrix = glm::translate(glm::mat4(1.0f), alien.position);
            renderObject(alien, MatrixID, ModelMatrixID, ViewMatrixID, textureID, ProjectionMatrix, ViewMatrix);
        }

        handleLaserAlienCollisions(aliens_vector);

        handleLaserMothershipCollision(motherShip);

        for (auto &laser : lasers)
        {
            updateLaser(laser, deltaTime);
        }

        // Render the lasers
        for (auto &laser : lasers)
        {
            renderLaser(laser, MatrixID, ModelMatrixID, ViewMatrixID, textureID, ProjectionMatrix, ViewMatrix);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();

    } while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
             glfwWindowShouldClose(window) == 0);

    cleanup(aliens_vector, playerShip, motherShip);

    glDeleteProgram(programID);
    glfwTerminate();
    return 0;
}
