// Include necessary standard and external libraries
#include <stdio.h>                  // Standard input/output operations
#include <stdlib.h>                 // Standard library functions
#include <vector>                   // Vector container from the Standard Template Library (STL)
#include <iostream>                 // Standard input/output stream for debugging/logging
#include <map>                      // Map container from STL for key-value pairs

// OpenGL, GLEW, and GLFW libraries for graphics rendering and window management
#include <GL/glew.h>                // OpenGL Extension Wrangler Library (GLEW) for OpenGL functionality
#include <GLFW/glfw3.h>             // GLFW for creating windows, handling input, etc.
GLFWwindow* window;                // Declare a pointer for the GLFW window

// GLM: OpenGL Mathematics library for matrix and vector operations
#include <glm/glm.hpp>              // GLM header for mathematical operations on vectors and matrices
#include <glm/gtc/matrix_transform.hpp> // GLM helper functions for matrix transformations

// Use glm's namespace for simplicity in writing matrix and vector types
using namespace glm;

// Include additional necessary headers from the project's common folder
#include "common/shader.hpp"        // Shader loading and compiling functions
#include "common/controls.hpp"      // Controls handling (e.g., keyboard and mouse input)
#include "common/vboindexer.hpp"    // Utility to optimize vertex buffer and index buffer creation
#include "common/texture.hpp"       // Texture loading and handling

// Include TinyObjLoader for loading .obj 3D model files
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"        // Header for tinyobjloader

// Include STB Image for texture loading from image files
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"              // Header for stb_image library used to load textures

// Macro definitions for debugging purposes
#define DEBUG true                  // Set to true to enable debug prints
#define DEBUG_PRINT(msg) if (DEBUG) {std::cout << msg << std::endl;}  // Print the message if debugging is enabled

// Declare external variables
extern GLFWwindow* window;           // Pointer to the GLFW window
extern glm::mat4 ViewMatrix;         // Camera view matrix for transforming objects
extern glm::mat4 ProjectionMatrix;   // Camera projection matrix (for perspective rendering)



///  Global Variables

// General use
float LEFTBOUNDARY = -50.0f;       // Left boundary of the movement area
float RIGHTBOUNDARY = 50.0f;       // Right boundary of the movement area
int nextObjectId = 0;              // A global counter to ensure each object has a unique identifier (ID)

// Alien Related
float alienSpeed = 0.02f;          // Speed at which aliens move horizontally
bool alienMovingRight = true;      // Boolean flag to indicate the current direction of alien movement (right or left)
float alienDropDistance = 0.5f;    // Distance that aliens move down when they hit a boundary

// MotherShip Related
bool mothershipAlive;       // Flag to check if the mothership is still alive
int mothershipHealth;              // Variable to hold the health of the mothership, can be adjusted
bool mothershipMovingRight = true; // Direction flag for the mothership (moving right or left)
const float mothershipSpeed = 0.05f; // Speed at which the mothership moves horizontally

//Player Related
const float PLAYER_SPEED = 20.0f;  // Speed at which the player can move
double lastShotTime = 0.0f;        // Stores the time when the last shot was fired (for cooldown purposes)
const float SHOT_COOLDOWN = 0.3f;  // Cooldown duration for shooting


// Define the structure to hold the cached data for OBJ modles
struct ObjCache
{
    std::vector<glm::vec3> vertices;            // Vertices of the object (positions of each point in space)
    std::vector<glm::vec2> uvs;                 // Texture coordinates for the object's surfaces
    std::vector<glm::vec3> normals;             // Normals for lighting calculations
    std::vector<tinyobj::material_t> materials; // Material data (e.g., color, specular, etc.)
    std::vector<std::string> textures;          // File paths to texture images
    std::vector<GLuint> textureIDs;             // OpenGL texture IDs associated with the textures
};

// Define the structure to represent each game object (such as player, alien, etc.)
struct GameObject
{
    std::string objFile;               // Path to the .obj file containing the 3D model
    std::string mtlFile;               // Path to the material (.mtl) file associated with the model
    std::vector<glm::vec3> vertices;   // Vertices of the object (positions in 3D space)
    std::vector<glm::vec2> uvs;        // Texture coordinates
    std::vector<glm::vec3> normals;    // Normal vectors used for lighting
    std::vector<tinyobj::material_t> materials; // Material properties for rendering
    std::vector<std::string> textures; // List of textures associated with the object
    std::vector<GLuint> textureIDs;    // OpenGL texture IDs for the object
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f); // Default position of the object
    glm::mat4 modelMatrix = glm::mat4(1.0f);  // Default identity matrix for the model (no transformation by default)
    GLuint vertexArrayID = 0;          // OpenGL Vertex Array Object (VAO) ID
    GLuint vertexBuffer = 0;           // OpenGL Vertex Buffer Object (VBO) for vertices
    GLuint uvBuffer = 0;               // OpenGL VBO for texture coordinates (UVs)
    GLuint normalBuffer = 0;           // OpenGL VBO for normals
    int id = -1;                       // Unique identifier for the object (default -1)
    std::string type;                  // Type of the object (e.g., "alien", "player", etc.)

    // Constructor to initialize the GameObject with default values
    GameObject()
        : position(glm::vec3(0.0f, 0.0f, 0.0f)),   // Default position at the origin
        modelMatrix(glm::mat4(1.0f)),             // Default model matrix (identity matrix, no transformations)
        vertexArrayID(0), vertexBuffer(0), uvBuffer(0), normalBuffer(0), id(-1) // Default to invalid or uninitialized OpenGL objects
    {
    }
};

// Define the Laser structure, which represents a laser shot fired by the player or enemies
struct Laser
{
    glm::vec3 direction = glm::vec3(0.0f, 1.0f, 0.0f); // Direction the laser moves in (default is upwards)
    float speed = 10.0f;                               // Speed at which the laser moves
    bool active = false;                               // Whether the laser is active and should be rendered or not
    bool player_friendly = true;                       // To specify if they are player (true) or alien frindly(false)

    GameObject obj;                                   // The laser itself is also a GameObject, allowing it to have geometry and a model
};

// Declare a cache to store parsed OBJ file data to avoid reloading the same file multiple times
std::map<std::string, ObjCache> objCache;  // Maps object file paths to their corresponding cache (vertices, textures, etc.)

// Declare a vector to hold all active lasers in the game
std::vector<Laser> lasers;  // Vector containing all active lasers (both player and enemy lasers)


//-------------------------------------------------------------------------------------------------
// Function to load textures and cache them to avoid reloading the same texture multiple times
GLuint loadTexture(const std::string& texturePath)
{
    int width, height, channels;
    // Print debug message with texture path
    DEBUG_PRINT("Attempting to load texture from: " << texturePath);

    // Load the texture data from the specified file path
    unsigned char* data = stbi_load(texturePath.c_str(), &width, &height, &channels, 0);
    if (!data) // Check if texture loading failed
    {
        // Print debug message with error details
        DEBUG_PRINT("Failed to load texture: " << texturePath);
        DEBUG_PRINT("stbi_error: " << stbi_failure_reason());
        return 0; // Return 0 (invalid texture) in case of failure
    }

    GLuint textureID;
    glGenTextures(1, &textureID); // Generate a texture ID
    glBindTexture(GL_TEXTURE_2D, textureID); // Bind the texture to the 2D texture target

    // Set texture parameters (wrapping and filtering modes)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // Wrap the texture in the S direction
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); // Wrap the texture in the T direction
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // Linear filtering when texture is minified
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // Linear filtering when texture is magnified

    // Check number of channels in the image (RGB or RGBA)
    if (channels == 3) // RGB texture
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    }
    else if (channels == 4) // RGBA texture
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    }

    glGenerateMipmap(GL_TEXTURE_2D); // Generate mipmaps for the texture
    stbi_image_free(data); // Free the loaded image data from memory

    // Print debug message indicating texture load success
    DEBUG_PRINT("Successfully loaded texture: " << texturePath);
    return textureID; // Return the texture ID
}


//-------------------------------------------------------------------------------------------------
// Function to load the OBJ file and its associated materials
// Caches data to avoid reloading the same object multiple times
bool OBJloadingfunction(const char* objpath, const char* mtlpath,
    std::vector<glm::vec3>& out_vertices,
    std::vector<glm::vec2>& out_uvs,
    std::vector<glm::vec3>& out_normals,
    std::vector<tinyobj::material_t>& out_materials,
    std::vector<std::string>& out_textures,
    std::vector<GLuint>& out_textureIDs)
{
    // Check if the OBJ file is already in the cache
    auto it = objCache.find(objpath);
    if (it != objCache.end()) // If cached data exists, use it
    {
        DEBUG_PRINT("Object Data Already Exists -- Using cached data!");
        // Retrieve cached data and assign it to output variables
        const ObjCache& cache = it->second;
        out_vertices = cache.vertices;
        out_uvs = cache.uvs;
        out_normals = cache.normals;
        out_materials = cache.materials;
        out_textures = cache.textures;
        out_textureIDs = cache.textureIDs;
        return true; // Return true since data is fetched from cache
    }

    // Proceed with loading the OBJ file if not cached
    tinyobj::attrib_t attrib; // Holds object attributes (vertices, normals, texcoords)
    std::vector<tinyobj::shape_t> shapes; // Stores shapes from the OBJ file
    std::vector<tinyobj::material_t> materials; // Stores material properties (color, texture)
    std::string warn, err; // To hold warning and error messages during the load process

    DEBUG_PRINT("Loading OBJ file: " << objpath);

    // Load the OBJ file and materials using TinyOBJ loader
    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, objpath, mtlpath);
    if (!warn.empty()) // Print warnings if any
    {
        std::cerr << "Warning: " << warn << std::endl;
    }
    if (!err.empty()) // Print errors if any
    {
        std::cerr << "Error: " << err << std::endl;
        return false; // Return false if loading fails
    }

    // Store materials for later use
    out_materials = materials;

    // Get the folder path for the materials to load textures correctly
    std::string mtlFolderPath = std::string(mtlpath);
    size_t lastSlash = mtlFolderPath.find_last_of('/');
    if (lastSlash != std::string::npos) // Extract the folder path of the material file
    {
        mtlFolderPath = mtlFolderPath.substr(0, lastSlash);
    }

    // Iterate over each material and check if it has a texture
    for (const auto& material : materials)
    {
        if (!material.diffuse_texname.empty()) // If a texture is specified
        {
            std::string fullTexturePath = mtlFolderPath + "/" + material.diffuse_texname; // Create full texture path
            out_textures.push_back(fullTexturePath); // Store texture path
            DEBUG_PRINT("Found texture: " << fullTexturePath);
        }
        else
        {
            out_textures.push_back(""); // No texture, store an empty string
        }
    }

    // Load textures for each material (if available)
    for (const auto& texturePath : out_textures)
    {
        if (!texturePath.empty()) // If texture path is not empty
        {
            GLuint textureID = loadTexture(texturePath); // Load the texture
            out_textureIDs.push_back(textureID); // Store the texture ID
        }
        else
        {
            out_textureIDs.push_back(0); // No texture, store 0 (invalid texture)
        }
    }

    // Iterate over the shapes and extract vertex data
    for (const auto& shape : shapes)
    {
        for (const auto& index : shape.mesh.indices) // Loop through mesh indices
        {
            glm::vec3 vertex;
            glm::vec2 uv;
            glm::vec3 normal;

            // Extract vertex position from attribute data
            vertex.x = attrib.vertices[3 * index.vertex_index + 0];
            vertex.y = attrib.vertices[3 * index.vertex_index + 1];
            vertex.z = attrib.vertices[3 * index.vertex_index + 2];
            out_vertices.push_back(vertex); // Store vertex position

            // Extract normal vector (if available) and store it
            if (index.normal_index >= 0)
            {
                normal.x = attrib.normals[3 * index.normal_index + 0];
                normal.y = attrib.normals[3 * index.normal_index + 1];
                normal.z = attrib.normals[3 * index.normal_index + 2];
                out_normals.push_back(normal);
            }

            // Extract texture coordinates (if available) and store them
            if (index.texcoord_index >= 0)
            {
                uv.x = attrib.texcoords[2 * index.texcoord_index + 0];
                uv.y = attrib.texcoords[2 * index.texcoord_index + 1];
                out_uvs.push_back(uv);
            }
        }
    }

    // Cache the loaded data for future use
    ObjCache cache;
    cache.vertices = out_vertices;
    cache.uvs = out_uvs;
    cache.normals = out_normals;
    cache.materials = out_materials;
    cache.textures = out_textures;
    cache.textureIDs = out_textureIDs;
    objCache[objpath] = cache; // Store the data in the cache

    // Print success message and return true
    DEBUG_PRINT("OBJ file loaded successfully!");
    return true;
}


//-------------------------------------------------------------------------------------------------
// Function to load game object data and initialize buffers
void loadGameObject(GameObject& obj)
{
    // Print the file being loaded to the debug log
    DEBUG_PRINT("Loading GameObject: " << obj.objFile);

    // Attempt to load the object file and its materials, if loading fails, print error
    if (!OBJloadingfunction(obj.objFile.c_str(), obj.mtlFile.c_str(), obj.vertices, obj.uvs, obj.normals, obj.materials, obj.textures, obj.textureIDs))
    {
        DEBUG_PRINT("Failed to load GameObject: " << obj.objFile);
        return; // Return early if loading fails
    }

    // Generate a new Vertex Array Object (VAO) for the game object to store vertex attributes
    glGenVertexArrays(1, &obj.vertexArrayID);
    glBindVertexArray(obj.vertexArrayID);

    // Create and bind a Vertex Buffer Object (VBO) for the vertex data (positions)
    glGenBuffers(1, &obj.vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, obj.vertexBuffer);
    // Fill the buffer with the vertex data (positions) from the object
    glBufferData(GL_ARRAY_BUFFER, obj.vertices.size() * sizeof(glm::vec3), &obj.vertices[0], GL_STATIC_DRAW);

    // Create and bind a VBO for the UV texture coordinates data
    glGenBuffers(1, &obj.uvBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, obj.uvBuffer);
    // Fill the UV buffer with the texture coordinates from the object
    glBufferData(GL_ARRAY_BUFFER, obj.uvs.size() * sizeof(glm::vec2), &obj.uvs[0], GL_STATIC_DRAW);

    // Create and bind a VBO for the normal vector data
    glGenBuffers(1, &obj.normalBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, obj.normalBuffer);
    // Fill the normal buffer with the normal vectors from the object
    glBufferData(GL_ARRAY_BUFFER, obj.normals.size() * sizeof(glm::vec3), &obj.normals[0], GL_STATIC_DRAW);

    // Iterate over the texture IDs and bind each texture if it's valid (non-zero ID)
    for (GLuint textureID : obj.textureIDs)
    {
        if (textureID != 0)
        {
            // Bind the texture if it is valid
            glBindTexture(GL_TEXTURE_2D, textureID);
        }
    }
}


//-------------------------------------------------------------------------------------------------
// Function to generate unique IDs for game objects
int generateUniqueID()
{
    // Increment and return the next available unique object ID
    return nextObjectId++;
}


//-------------------------------------------------------------------------------------------------
// Function to load the player ship
void createPlayer(GameObject& playerShip)
{
    // Log the creation process of the player
    DEBUG_PRINT("Creating player...");

    // Set the player's object file and material file for loading
    playerShip.objFile = "obj/player.obj";
    playerShip.mtlFile = "obj";  // Same texture folder as other objects

    // Set the player's initial position in the game world
    playerShip.position = glm::vec3(0.0f, -35.0f, 0.0f); // Position the player lower in the scene

    // Generate a unique ID for the player
    playerShip.id = generateUniqueID();

    // Set the player type to "Player"
    playerShip.type = "Player";

    // Load the player object data
    loadGameObject(playerShip);

    // Log the successful creation of the player with its unique ID
    DEBUG_PRINT("Player created with ID: " << playerShip.id);
    DEBUG_PRINT("-----------------------------------------");
}


//-------------------------------------------------------------------------------------------------
// Function to load the mothership
void createMothership(GameObject& motherShip)
{
    // Log the creation process of the mothership
    DEBUG_PRINT("Creating mothership...");

    // Set the mothership's object and material file
    motherShip.objFile = "obj/mothership.obj";
    motherShip.mtlFile = "obj"; // Assuming same texture folder as other objects

    // Set the mothership's initial position in the game world
    motherShip.position = glm::vec3(0.0f, 32.0f, 0.0f);

    // Generate a unique ID for the mothership
    motherShip.id = generateUniqueID();

    // Set the mothership type to "MotherShip"
    motherShip.type = "MotherShip";

    // Set the mothership's initial state to alive when the game starts
    mothershipAlive = true;

    // Set initial health value (this can be adjusted later)
    int mothershipHealth = 10;

    // Load the mothership object data
    loadGameObject(motherShip);

    // Log the successful creation of the mothership with its unique ID
    DEBUG_PRINT("Mothership created with ID: " << motherShip.id);
    DEBUG_PRINT("-----------------------------------------");
}


//-------------------------------------------------------------------------------------------------
// Function to create a laser
void createLaser(Laser& laser, const glm::vec3& playerPosition, const glm::vec3& startPos, bool player_shot, const glm::vec3& alienPosition = glm::vec3(0.0f, 0.0f, 0.0f))
{
    // Set the laser's position in the game world (starts from the player's ship)
    laser.obj.position = startPos;

    // Set the laser's object file and material file for loading
    laser.obj.objFile = "obj/laser.obj";
    laser.obj.mtlFile = "obj"; // Same texture folder as other objects

    if (player_shot == true)
    {
        laser.player_friendly = true; // Set as Player Ally
        laser.obj.type = "Player Laser"; // Set the object type as " Player Laser"
        laser.direction = glm::vec3(0.0f, 1.0f, 0.0f); // Laser moves upwards
    }
    else if (player_shot == false)
    {
        laser.player_friendly = false; // Set as Player Enemy
        laser.obj.type = "Enemy Laser"; // Set the object type as "Alien Laser"

        if (alienPosition != glm::vec3(0.0f, 0.0f, 0.0f))
        {
            // Calculate the direction from the alien to the player if alien position is provided
            glm::vec3 directionToPlayer = playerPosition - alienPosition; // Vector from alien to player
            laser.direction = glm::normalize(directionToPlayer); // Normalize the vector to get direction
        }
        else
        {
            // Default direction if no alien position is provided (e.g., laser fired from player)
            laser.direction = glm::vec3(0.0f, -1.0f, 0.0f); // Move upwards by default
        }

    }

    // Load the laser object data
    loadGameObject(laser.obj);

    // Set the laser's active state to true, meaning it's ready for use
    laser.active = true;
}


//-------------------------------------------------------------------------------------------------
// Function to create aliens dynamically with flexibility
void createAliens(std::vector<GameObject>& aliens_vector, int rows = 5, int cols = 4, float spacing = 5.0f, const glm::vec3& startPos = glm::vec3(0.0f, 25.0f, 0.0f))
{
    // Define an array of alien models to be assigned dynamically to aliens
    std::vector<std::string> alienModels = { "obj/alien1.obj", "obj/alien2.obj", "obj/alien3.obj" };
    DEBUG_PRINT("Creating aliens...");

    // Loop through the rows and columns to create aliens dynamically
    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            // Create a new GameObject instance for each alien
            GameObject alien;

            // Assign an alien model based on the current row, cycling through models
            alien.objFile = alienModels[row % alienModels.size()];
            alien.mtlFile = "obj/"; // Assuming all aliens share the same material folder
            alien.type = "Alien";    // Set the object type as "Alien"

            // Position the alien in a grid formation, adjusting for spacing and start position
            alien.position = startPos + glm::vec3(col * spacing, -row * spacing, 0.0f);

            // Generate a unique ID for each alien
            alien.id = generateUniqueID();

            // Load the alien's object data
            loadGameObject(alien);

            // Add the newly created alien to the aliens vector
            aliens_vector.push_back(alien);
        }
    }

    // Log how many aliens were created
    DEBUG_PRINT("Created: " << aliens_vector.size() << " Aliens!");
}


//-------------------------------------------------------------------------------------------------
// Function to initialize the game objects (Player, Mothership, Aliens)
void initializeGameObjects(std::vector<GameObject>& aliens_vector, GameObject& playerShip, GameObject& motherShip)
{
    // Call functions to create the player ship, mothership, and alien swarm
    createPlayer(playerShip);
    createMothership(motherShip);
    createAliens(aliens_vector, 6, 6); // Initialize 6 rows and 5 columns of aliens
}


//-------------------------------------------------------------------------------------------------
// Function to render any game object
void renderObject(const GameObject& obj, GLuint MatrixID, GLuint ModelMatrixID, GLuint ViewMatrixID, GLuint textureID, const glm::mat4& ProjectionMatrix, const glm::mat4& ViewMatrix)
{
    // Set up the Model-View-Projection (MVP) matrix by multiplying the projection, view, and model matrices
    glm::mat4 MVP = ProjectionMatrix * ViewMatrix * obj.modelMatrix;

    // Send the MVP transformation matrix to the shader for rendering
    glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);

    // Send the individual model matrix to the shader
    glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &obj.modelMatrix[0][0]);

    // Send the view matrix to the shader
    glUniformMatrix4fv(ViewMatrixID, 1, GL_FALSE, &ViewMatrix[0][0]);

    // Bind textures associated with the object
    for (size_t i = 0; i < obj.textures.size(); i++)
    {
        GLuint texID = obj.textureIDs[i];
        if (texID != 0) // Only bind if the texture ID is valid
        {
            // Activate the texture unit and bind the texture for this index
            glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(i));
            glBindTexture(GL_TEXTURE_2D, texID);

            // Inform the shader which texture unit to use
            glUniform1i(textureID, static_cast<GLuint>(i));
        }
    }

    // Bind the Vertex Array Object (VAO) to prepare for rendering
    glBindVertexArray(obj.vertexArrayID);

    // Enable and configure the vertex attribute for positions
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, obj.vertexBuffer);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

    // Enable and configure the vertex attribute for texture coordinates (UVs)
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, obj.uvBuffer);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

    // Enable and configure the vertex attribute for normals
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, obj.normalBuffer);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

    // Draw the object using the vertex array
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(obj.vertices.size()));

    // Disable the vertex attribute arrays after rendering
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
}


//-------------------------------------------------------------------------------------------------
// Function to clean up OpenGL resources for a GameObject
void cleanupGameObject(GameObject& obj)
{
    // Log the cleanup process of the GameObject with its ID and type
    DEBUG_PRINT("Cleaning up GameObject With ID = " << obj.id << " and Type = " << obj.type);

    // Delete OpenGL buffers associated with the object
    if (obj.vertexBuffer)
    {
        glDeleteBuffers(1, &obj.vertexBuffer);
        obj.vertexBuffer = 0; // Reset the buffer ID
    }
    if (obj.uvBuffer)
    {
        glDeleteBuffers(1, &obj.uvBuffer);
        obj.uvBuffer = 0; // Reset the buffer ID
    }
    if (obj.normalBuffer)
    {
        glDeleteBuffers(1, &obj.normalBuffer);
        obj.normalBuffer = 0; // Reset the buffer ID
    }

    // Delete the Vertex Array Object (VAO)
    if (obj.vertexArrayID)
    {
        glDeleteVertexArrays(1, &obj.vertexArrayID);
        obj.vertexArrayID = 0; // Reset the VAO ID
    }

    // Delete textures associated with the object
    for (GLuint& textureID : obj.textureIDs)
    {
        if (textureID)
        {
            glDeleteTextures(1, &textureID);
            textureID = 0; // Reset the texture ID
        }
    }
}


//-------------------------------------------------------------------------------------------------
// General cleanup function to clear all resources
void cleanup(std::vector<GameObject>& aliens, GameObject& playerShip, GameObject& motherShip)
{
    // Start of the cleanup process
    DEBUG_PRINT("-----------------------------------------");

    // Clean up the player ship resources
    cleanupGameObject(playerShip);

    // If the mothership is alive, clean up its resources
    if (mothershipAlive)
    {
        cleanupGameObject(motherShip); // Clean up mothership only if alive
    }

    // Clean up all aliens
    for (auto& alien : aliens)
    {
        cleanupGameObject(alien);
    }
    aliens.clear(); // Clear the alien vector after cleanup

    // Clean up lasers and clear their vector
    for (auto& laser : lasers)
    {
        cleanupGameObject(laser.obj); // Clean up the laser objects
    }
    lasers.clear(); // Clear the laser vector

    // Clear the object cache to free up memory
    objCache.clear();

    // End of the cleanup process
    DEBUG_PRINT("Cleanup complete!");
    DEBUG_PRINT("-----------------------------------------");
}


//-------------------------------------------------------------------------------------------------
// Function to handle player movement based on user input
void handlePlayerMovement(GameObject& player, float deltaTime)
{
    // Move player left when 'A' key is pressed
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    {
        player.position.x -= PLAYER_SPEED * deltaTime;
    }

    // Move player right when 'D' key is pressed
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    {
        player.position.x += PLAYER_SPEED * deltaTime;
    }

    // Ensure player stays within the screen boundaries
    if (player.position.x < LEFTBOUNDARY)
    {
        player.position.x = LEFTBOUNDARY; // Prevent movement beyond the left boundary
    }
    if (player.position.x > RIGHTBOUNDARY)
    {
        player.position.x = RIGHTBOUNDARY; // Prevent movement beyond the right boundary
    }

    // Fire a laser when the Spacebar is pressed and cooldown time has passed
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && (glfwGetTime() - lastShotTime) >= SHOT_COOLDOWN)
    {
        Laser newLaser;
        createLaser(newLaser, player.position, player.position + glm::vec3(0.0f, 2.0f, 0.0f), true); // Fire laser above the player
        lasers.push_back(newLaser); // Add new laser to the lasers array
        lastShotTime = glfwGetTime(); // Update last shot time for cooldown management
    }
}


//-------------------------------------------------------------------------------------------------
// Function to update the positions of aliens
void updateAlienPositions(std::vector<GameObject>& aliens_vector)
{
    bool hitBoundary = false;

    // Check if any alien has hit the boundary
    for (auto& alien : aliens_vector)
    {
        // If alien moves beyond the right boundary, mark it to reverse direction
        if (alienMovingRight && alien.position.x > RIGHTBOUNDARY)
        {
            hitBoundary = true;
        }
        // If alien moves beyond the left boundary, mark it to reverse direction
        if (!alienMovingRight && alien.position.x < LEFTBOUNDARY)
        {
            hitBoundary = true;
        }
    }

    // Reverse direction and move down if a boundary is hit
    if (hitBoundary)
    {
        alienMovingRight = !alienMovingRight; // Reverse the alien movement direction
        for (auto& alien : aliens_vector)
        {
            alien.position.y -= alienDropDistance; // Drop aliens down after reversing direction
        }
    }

    // Update positions of aliens based on the current movement direction
    float direction = alienMovingRight ? 1.0f : -1.0f; // Set direction for movement (right or left)
    for (auto& alien : aliens_vector)
    {
        alien.position.x += alienSpeed * direction; // Move aliens horizontally
    }
}


//-------------------------------------------------------------------------------------------------
// Function to update the mothership's position
void updateMothershipPosition(GameObject& motherShip)
{
    // Reverse direction if mothership hits screen boundaries
    if (motherShip.position.x < LEFTBOUNDARY || motherShip.position.x > RIGHTBOUNDARY)
    {
        mothershipMovingRight = !mothershipMovingRight; // Reverse mothership movement direction
    }

    // Move the mothership based on its direction
    if (mothershipMovingRight)
    {
        motherShip.position.x += mothershipSpeed; // Move right
    }
    else
    {
        motherShip.position.x -= mothershipSpeed; // Move left
    }
}


//-------------------------------------------------------------------------------------------------
// Function to update laser position
void updateLaser(Laser& laser, float deltaTime)
{
    if (!laser.active)
        return; // Skip if laser is inactive


    // Move the laser in its direction based on speed and delta time
    laser.obj.position += laser.direction * laser.speed * deltaTime;



    // Deactivate laser if it moves off the screen (y-axis or x-axis exceeds a certain limit)
    if (laser.obj.position.y > 35.0f || laser.obj.position.y < -35.0f || laser.obj.position.x > 55.0f || laser.obj.position.x < -55.0f)
    {
        laser.active = false; // Laser is no longer active
    }

}


//-------------------------------------------------------------------------------------------------
// Function to render a laser if it is active
void renderLaser(Laser& laser, GLuint MatrixID, GLuint ModelMatrixID, GLuint ViewMatrixID, GLuint textureID, const glm::mat4& ProjectionMatrix, const glm::mat4& ViewMatrix)
{
    // Only render laser if it's active
    if (laser.active)
    {
        // Update the model matrix to translate the laser to its current position
        laser.obj.modelMatrix = glm::translate(glm::mat4(1.0f), laser.obj.position);
        renderObject(laser.obj, MatrixID, ModelMatrixID, ViewMatrixID, textureID, ProjectionMatrix, ViewMatrix); // Call to render the laser object
    }
}


//-------------------------------------------------------------------------------------------------
// Function to check if a laser collides with an alien
bool checkLaserAlienCollision(const Laser& laser, GameObject& alien)
{

    if (laser.player_friendly == false)
    {
        return false; // Skip if laser is friendly to object
    }


    // Calculate the distance between the laser and the alien
    float distance = glm::length(laser.obj.position - alien.position);

    // Assume a collision threshold distance for laser and alien
    float threshold = 2.0f;

    // Return true if the laser collides with the alien based on the distance
    return distance < threshold;
}


//-------------------------------------------------------------------------------------------------
// Function to handle collisions between lasers and aliens
void handleLaserAlienCollisions(std::vector<GameObject>& aliens)
{
    // Iterate over all lasers and check for collisions
    for (auto laserIt = lasers.begin(); laserIt != lasers.end();)
    {
        if (!laserIt->active)
        {
            // Remove inactive lasers from the array
            laserIt = lasers.erase(laserIt);
        }
        else
        {
            // Check if laser collides with any alien
            for (auto it = aliens.begin(); it != aliens.end();)
            {
                if (checkLaserAlienCollision(*laserIt, *it))
                {
                    it = aliens.erase(it);   // Remove alien from the array on collision
                    laserIt->active = false; // Deactivate the laser after collision
                    break;                   // Stop checking once the laser hits an alien
                }
                else
                {
                    ++it; // Continue checking for other aliens
                }
            }

            ++laserIt; // Move to the next laser
        }
    }
}


//-------------------------------------------------------------------------------------------------
// Function to check if a laser collides with the mothership
bool checkLaserMothershipCollision(const Laser& laser, GameObject& motherShip)
{

    if (laser.player_friendly == false)
    {
        return false; // Skip if laser is friendly to object
    }

    if (!mothershipAlive)
        return false; // Skip if mothership is not alive

    // Calculate the distance between the laser and the mothership
    float distance = glm::length(laser.obj.position - motherShip.position);

    // Define a threshold for collision detection (can adjust as necessary)
    float threshold = 2.0f;

    // Return true if laser collides with the mothership
    return distance < threshold;
}


//-------------------------------------------------------------------------------------------------
// Function to handle laser collisions with the mothership
void handleLaserMothershipCollision(GameObject& mothership)
{
    // Iterate over all lasers and check for collision with the mothership
    for (auto laserIt = lasers.begin(); laserIt != lasers.end();)
    {
        if (!laserIt->active)
        {
            // Remove inactive lasers
            laserIt = lasers.erase(laserIt);
        }
        else
        {
            // Check for collision with the mothership
            if (checkLaserMothershipCollision(*laserIt, mothership))
            {
                mothershipHealth -= 1;   // Decrease mothership health on hit
                laserIt->active = false; // Deactivate the laser after collision

                // Check if mothership is destroyed
                if (mothershipHealth <= 0)
                {
                    mothershipAlive = false; // Set mothership as destroyed
                    DEBUG_PRINT("Mothership Destroyed!");
                    cleanupGameObject(mothership); // Clean up mothership resources
                }

                break; // Stop checking once a laser hits the mothership
            }
            else
            {
                ++laserIt; // Move to the next laser
            }
        }
    }
}



//-------------------------------------------------------------------------------------------------
// Function to handle alien laser firing
void handleAlienLaserFiring(std::vector<GameObject>& aliens_vector, const glm::vec3& playerPosition, int value, GameObject& player)
{
    for (auto& alien : aliens_vector)
    {
        // Random chance for each alien to fire
        if (rand() % 150000 < value) //  chance for each alien to fire 
        {
            Laser newLaser;
            createLaser(newLaser, player.position, alien.position + glm::vec3(0.0f, -2.0f, 0.0f), false, alien.position);
            lasers.push_back(newLaser); // Add new laser to the lasers array

        }
    }
}

//-------------------------------------------------------------------------------------------------
// Function to handle mothership laser firing
void handleMothershipLaserFiring(GameObject& motherShip, int value, GameObject& player)
{
    // Random chance for mothership to fire
    if (rand() % 300 < value) //  chance for mothership to fire 
    {
        Laser newLaser;
        createLaser(newLaser, player.position, motherShip.position + glm::vec3(0.0f, -2.0f, 0.0f), false);
        lasers.push_back(newLaser); // Add laser to the list of lasers
    }
}




//-------------------------------------------------------------------------------------------------
// Main function that runs the program
int main(void)
{
    // Initialize GLFW
    if (!glfwInit())
    {
        DEBUG_PRINT("Failed to initialize GLFW!"); // Error message if GLFW initialization fails
        return -1;
    }

    // Set OpenGL context settings (version and profile)
    glfwWindowHint(GLFW_SAMPLES, 4); // Anti-aliasing
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // Set OpenGL version (major)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3); // Set OpenGL version (minor)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Make OpenGL forward-compatible
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // Use core OpenGL profile

    // Create a GLFW window
    window = glfwCreateWindow(1920, 1080, "Space Invaders | Trabalho Final", NULL, NULL);
    if (window == NULL)
    {
        DEBUG_PRINT("Failed to open GLFW window!"); // Error message if window creation fails
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window); // Make the context of the window current
    glewExperimental = true;
    if (glewInit() != GLEW_OK)
    {
        DEBUG_PRINT("Failed to initialize GLEW!"); // Error message if GLEW initialization fails
        glfwTerminate();
        return -1;
    }

    // Set input modes for GLFW (sticky keys and disable cursor)
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE); // Ensures keys remain pressed after being detected
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // Hides the cursor
    glfwSetCursorPos(window, 1920 / 2, 1080 / 2); // Position the cursor at the center of the window

    // OpenGL settings
    glClearColor(0.15f, 0.15f, 0.15f, 0.0f); // Set background color
    glEnable(GL_DEPTH_TEST); // Enable depth testing for 3D rendering
    glDepthFunc(GL_LESS); // Use less than comparison for depth testing
    glEnable(GL_CULL_FACE); // Enable face culling (only render front faces)

    // Load shaders (vertex and fragment shaders)
    GLuint programID = LoadShaders("main.vertexshader", "main.fragmentshader");
    DEBUG_PRINT("Shaders loaded successfully!");
    DEBUG_PRINT("-----------------------------------------");

    // Get uniform locations for transformation matrices and texture
    GLuint MatrixID = glGetUniformLocation(programID, "MVP");
    GLuint ViewMatrixID = glGetUniformLocation(programID, "V");
    GLuint ModelMatrixID = glGetUniformLocation(programID, "M");
    GLuint textureID = glGetUniformLocation(programID, "textureSampler");

    // Game objects (player ship, aliens, mothership)
    std::vector<GameObject> aliens_vector;
    GameObject playerShip, motherShip;

    // Initialize all game objects (player, mothership, aliens)
    initializeGameObjects(aliens_vector, playerShip, motherShip);

    double lastTime = glfwGetTime(); // Store the initial time for deltaTime calculations

    int alien_laser_timer = 1;
    int mothership_laser_timer = 5;


    // Main game loop
    do
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear the screen
        glUseProgram(programID); // Use the shader program

        // Calculate deltaTime for smooth movement
        double currentTime = glfwGetTime(); // Get the current time
        float deltaTime = static_cast<float>(currentTime - lastTime); // Compute deltaTime
        lastTime = currentTime; // Update lastTime

        // Calculate the view and projection matrices
        computeMatricesFromInput(playerShip.position);
        glm::mat4 ProjectionMatrix = getProjectionMatrix();
        glm::mat4 ViewMatrix = getViewMatrix();

        // Handle player movement (keyboard input)
        handlePlayerMovement(playerShip, deltaTime);

        // Update alien positions
        updateAlienPositions(aliens_vector);

        // Render player ship
        playerShip.modelMatrix = glm::translate(glm::mat4(1.0f), playerShip.position); // Update model matrix
        playerShip.modelMatrix = glm::scale(playerShip.modelMatrix, glm::vec3(0.5f, 0.5f, 0.5f)); // Scale player ship
        renderObject(playerShip, MatrixID, ModelMatrixID, ViewMatrixID, textureID, ProjectionMatrix, ViewMatrix); // Render player ship

        // Render mothership only if it's alive
        if (mothershipAlive)
        {
            updateMothershipPosition(motherShip); // Update mothership position
            motherShip.modelMatrix = glm::translate(glm::mat4(1.0f), motherShip.position); // Update model matrix
            motherShip.modelMatrix = glm::scale(motherShip.modelMatrix, glm::vec3(0.5f, 0.5f, 0.5f)); // Scale mothership
            renderObject(motherShip, MatrixID, ModelMatrixID, ViewMatrixID, textureID, ProjectionMatrix, ViewMatrix); // Render mothership


            // Handle mothership laser firing
            handleMothershipLaserFiring(motherShip, mothership_laser_timer, playerShip);



            // Handle laser-mothership collisions
            handleLaserMothershipCollision(motherShip);


        }

        // Render each alien dynamically
        for (auto& alien : aliens_vector)
        {
            alien.modelMatrix = glm::translate(glm::mat4(1.0f), alien.position); // Update alien position
            renderObject(alien, MatrixID, ModelMatrixID, ViewMatrixID, textureID, ProjectionMatrix, ViewMatrix); // Render alien


            // Handle laser-alien collisions
            handleLaserAlienCollisions(aliens_vector);

            // Handle alien laser firing
            handleAlienLaserFiring(aliens_vector, playerShip.position, alien_laser_timer, playerShip);


        }

        // Update and render lasers
        for (auto& laser : lasers)
        {
            updateLaser(laser, deltaTime); // Update laser position
        }

        // Render lasers
        for (auto& laser : lasers)
        {
            renderLaser(laser, MatrixID, ModelMatrixID, ViewMatrixID, textureID, ProjectionMatrix, ViewMatrix); // Render each laser
        }

        glfwSwapBuffers(window); // Swap buffers to update the screen
        glfwPollEvents(); // Process input events

    } while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS && // Exit if the ESC key is pressed
        glfwWindowShouldClose(window) == 0); // Exit if the window is closed

    // Clean up game objects and resources
    cleanup(aliens_vector, playerShip, motherShip);

    glDeleteProgram(programID); // Delete shader program
    glfwTerminate(); // Terminate GLFW
    return 0; // Exit the program
}



