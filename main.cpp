#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <iostream>
#include <map>

// OpenGL Extension Wrangler
#include <GL/glew.h>

// Graphics Library Framework
#include <GLFW/glfw3.h>
GLFWwindow* window;

// OpenGL Mathematics
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;

// Utility headers
#include <common/shader.hpp>
#include <common/controls.hpp>
#include <common/vboindexer.hpp>
#include <common/texture.hpp>

// Include TinyObjLoader
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define DEBUG true 

#define DEBUG_PRINT(msg) if (DEBUG) { std::cout << msg << std::endl; }



// Cache structure to hold parsed OBJ data
struct ObjCache {
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec3> normals;
    std::vector<tinyobj::material_t> materials;
    std::vector<std::string> textures;
    std::vector<GLuint> textureIDs;
};

// GameObject structure
struct GameObject {
    std::string objFile;
    std::string mtlFile;
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec3> normals;
    std::vector<tinyobj::material_t> materials;
    std::vector<std::string> textures;
    std::vector<GLuint> textureIDs;
    glm::vec3 position;
    glm::mat4 modelMatrix;
    GLuint vertexArrayID;
    GLuint vertexBuffer;
    GLuint uvBuffer;
    GLuint normalBuffer;
};

// Texture cache to avoid reloading the same texture multiple times
std::map<std::string, GLuint> textureCache;

// Cache to store parsed OBJ data
std::map<std::string, ObjCache> objCache;

// Function to load textures and cache them
GLuint loadTexture(const std::string& texturePath) {
    if (textureCache.find(texturePath) != textureCache.end()) {
        DEBUG_PRINT("Texture already loaded: " << texturePath);
        return textureCache[texturePath];
    }

    int width, height, channels;
    DEBUG_PRINT("Attempting to load texture from: " << texturePath);
    unsigned char* data = stbi_load(texturePath.c_str(), &width, &height, &channels, 0);
    if (!data) {
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

    if (channels == 3) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    }
    else if (channels == 4) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    }

    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);

    textureCache[texturePath] = textureID;  // Cache the loaded texture

    DEBUG_PRINT("Successfully loaded texture: " << texturePath);
    return textureID;
}

// Function to load the OBJ file and materials, and store the data
bool OBJloadingfunction(const char* objpath, const char* mtlpath,
    std::vector<glm::vec3>& out_vertices,
    std::vector<glm::vec2>& out_uvs,
    std::vector<glm::vec3>& out_normals,
    std::vector<tinyobj::material_t>& out_materials,
    std::vector<std::string>& out_textures,
    std::vector<GLuint>& out_textureIDs) {

    // Check if the OBJ file is already cached
    auto it = objCache.find(objpath);
    if (it != objCache.end()) {
        // Use cached data
        const ObjCache& cache = it->second;
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

    if (!warn.empty()) {
        std::cerr << "Warning: " << warn << std::endl;
    }
    if (!err.empty()) {
        std::cerr << "Error: " << err << std::endl;
        return false;
    }

    out_materials = materials;

    std::string mtlFolderPath = std::string(mtlpath);
    size_t lastSlash = mtlFolderPath.find_last_of('/');
    if (lastSlash != std::string::npos) {
        mtlFolderPath = mtlFolderPath.substr(0, lastSlash);
    }

    for (const auto& material : materials) {
        if (!material.diffuse_texname.empty()) {
            std::string fullTexturePath = mtlFolderPath + "/" + material.diffuse_texname;
            out_textures.push_back(fullTexturePath);
            DEBUG_PRINT("Found texture: " << fullTexturePath);
        }
        else {
            out_textures.push_back("");
        }
    }

    for (const auto& texturePath : out_textures) {
        if (!texturePath.empty()) {
            GLuint textureID = loadTexture(texturePath);
            out_textureIDs.push_back(textureID);
        }
        else {
            out_textureIDs.push_back(0);
        }
    }

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            glm::vec3 vertex;
            glm::vec2 uv;
            glm::vec3 normal;

            vertex.x = attrib.vertices[3 * index.vertex_index + 0];
            vertex.y = attrib.vertices[3 * index.vertex_index + 1];
            vertex.z = attrib.vertices[3 * index.vertex_index + 2];
            out_vertices.push_back(vertex);

            if (index.normal_index >= 0) {
                normal.x = attrib.normals[3 * index.normal_index + 0];
                normal.y = attrib.normals[3 * index.normal_index + 1];
                normal.z = attrib.normals[3 * index.normal_index + 2];
                out_normals.push_back(normal);
            }

            if (index.texcoord_index >= 0) {
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

    DEBUG_PRINT("OBJ file loaded successfully.");
    return true;
}

// Function to load game object data and initialize buffers
void loadGameObject(GameObject& obj) {
    DEBUG_PRINT("Loading GameObject: " << obj.objFile);
    if (!OBJloadingfunction(obj.objFile.c_str(), obj.mtlFile.c_str(), obj.vertices, obj.uvs, obj.normals, obj.materials, obj.textures, obj.textureIDs)) {
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
    for (GLuint textureID : obj.textureIDs) {
        if (textureID != 0) {
            glBindTexture(GL_TEXTURE_2D, textureID);
        }
    }
}

// Function to load the player ship
void createPlayer(GameObject& playerShip) {
    DEBUG_PRINT("Creating player...)");
    playerShip.objFile = "obj/player.obj";
    playerShip.mtlFile = "obj";
    playerShip.position = glm::vec3(2.0f, -22.0f, 0.0f);  // Set player lower in the scene
    loadGameObject(playerShip);
    DEBUG_PRINT("Player created");
}

// Function to load the mothership
void createMothership(GameObject& motherShip) {
    DEBUG_PRINT("Creating mothership...");
    motherShip.objFile = "obj/mothership.obj";
    motherShip.mtlFile = "obj";
    motherShip.position = glm::vec3(1.0f, 12.0f, 0.0f);
    loadGameObject(motherShip);
    DEBUG_PRINT("Mothership created");
}

// Function to create aliens dynamically
void createAliens(std::vector<GameObject>& aliens_vector) {
    const float spacing = 5.0f;
    const glm::vec3 startPos(-6.0f, 4.0f, 0.0f);
    DEBUG_PRINT("Creating aliens...");

    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 4; ++col) {
            GameObject alien;
            if (row < 2) {
                alien.objFile = "obj/alien1.obj";
                alien.mtlFile = "obj/";
            }
            else if (row < 4) {
                alien.objFile = "obj/alien2.obj";
                alien.mtlFile = "obj/";
            }
            else {
                alien.objFile = "obj/alien3.obj";
                alien.mtlFile = "obj/";
            }

            alien.position = startPos + glm::vec3(col * spacing, -row * spacing, 0.0f);
            loadGameObject(alien);
            aliens_vector.push_back(alien);
        }
    }

    DEBUG_PRINT("Aliens created: " << aliens_vector.size());
}

// Function to initialize the game objects (Player, Mothership, Aliens)
void initializeGameObjects(std::vector<GameObject>& aliens_vector, GameObject& playerShip, GameObject& motherShip) {
    createPlayer(playerShip);
    createMothership(motherShip);
    createAliens(aliens_vector);
}





// Function to render any game object
void renderObject(const GameObject& obj, GLuint MatrixID, GLuint ModelMatrixID, GLuint ViewMatrixID, GLuint textureID, const glm::mat4& ProjectionMatrix, const glm::mat4& ViewMatrix) {
    // Set up model matrix for translation
    glm::mat4 MVP = ProjectionMatrix * ViewMatrix * obj.modelMatrix;

    // Send the transformation matrices to the shaders
    glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);
    glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &obj.modelMatrix[0][0]);
    glUniformMatrix4fv(ViewMatrixID, 1, GL_FALSE, &ViewMatrix[0][0]);

    // Bind textures
    for (size_t i = 0; i < obj.textures.size(); i++) {
        if (obj.textureIDs[i] != 0) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, obj.textureIDs[i]);
            glUniform1i(textureID, i);
        }
    }

    // Bind VAO and draw
    glBindVertexArray(obj.vertexArrayID);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, obj.vertexBuffer);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, obj.uvBuffer);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, obj.normalBuffer);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

    // Draw the object
    glDrawArrays(GL_TRIANGLES, 0, obj.vertices.size());

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
}








// Constants for player movement
const float PLAYER_SPEED = 5.0f; // Adjust for desired speed
const float PLAYER_BOUNDARY_LEFT = -10.0f; // Adjust to your scene boundary
const float PLAYER_BOUNDARY_RIGHT = 10.0f;  // Adjust to your scene boundary

// Function to handle player movement
void handlePlayerMovement(GameObject& player, float deltaTime) {
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        player.position.x -= PLAYER_SPEED * deltaTime;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        player.position.x += PLAYER_SPEED * deltaTime;
    }

    // Ensure player stays within boundaries
    if (player.position.x < PLAYER_BOUNDARY_LEFT) {
        player.position.x = PLAYER_BOUNDARY_LEFT;
    }
    if (player.position.x > PLAYER_BOUNDARY_RIGHT) {
        player.position.x = PLAYER_BOUNDARY_RIGHT;
    }
}

float alienSpeed = 0.02f;  // Speed of alien movement
float alienBoundaryLeft = -10.0f;  // Left boundary
float alienBoundaryRight = 10.0f;  // Right boundary
bool alienMovingRight = true;  // Current direction of aliens
float alienDropDistance = 0.5f;  // Distance to move down when hitting a boundary


void updateAlienPositions(std::vector<GameObject>& aliens_vector) {
    bool hitBoundary = false;

    // Check for boundary collision
    for (auto& alien : aliens_vector) {
        if (alienMovingRight && alien.position.x > alienBoundaryRight) {
            hitBoundary = true;
        }
        if (!alienMovingRight && alien.position.x < alienBoundaryLeft) {
            hitBoundary = true;
        }
    }

    // If a boundary is hit, reverse direction and move down
    if (hitBoundary) {
        alienMovingRight = !alienMovingRight;  // Reverse direction
        for (auto& alien : aliens_vector) {
            alien.position.y -= alienDropDistance;  // Move down
        }
    }

    // Update positions based on the direction
    float direction = alienMovingRight ? 1.0f : -1.0f;
    for (auto& alien : aliens_vector) {
        alien.position.x += alienSpeed * direction;
    }
}

// Function to clean up OpenGL resources for a GameObject
void cleanupGameObject(GameObject& obj) {
    DEBUG_PRINT("Cleaning up GameObject: " << obj.objFile);

    // Delete OpenGL buffers
    if (obj.vertexBuffer) {
        glDeleteBuffers(1, &obj.vertexBuffer);
        obj.vertexBuffer = 0;
    }
    if (obj.uvBuffer) {
        glDeleteBuffers(1, &obj.uvBuffer);
        obj.uvBuffer = 0;
    }
    if (obj.normalBuffer) {
        glDeleteBuffers(1, &obj.normalBuffer);
        obj.normalBuffer = 0;
    }

    // Delete VAO
    if (obj.vertexArrayID) {
        glDeleteVertexArrays(1, &obj.vertexArrayID);
        obj.vertexArrayID = 0;
    }

    // Clear texture IDs
    for (GLuint& textureID : obj.textureIDs) {
        if (textureID) {
            glDeleteTextures(1, &textureID);
            textureID = 0;
        }
    }
}

// General cleanup function to clear all resources
void cleanup(std::vector<GameObject>& aliens, GameObject& playerShip, GameObject& motherShip) {
    // Cleanup aliens
    for (auto& alien : aliens) {
        cleanupGameObject(alien);
    }
    aliens.clear();

    // Cleanup player ship
    cleanupGameObject(playerShip);

    // Cleanup mothership
    cleanupGameObject(motherShip);

    // Clear OpenGL texture cache
    for (auto it = textureCache.begin(); it != textureCache.end(); ++it) {
        GLuint textureID = it->second;
        glDeleteTextures(1, &textureID);
    }
    textureCache.clear();

    // Clear object cache
    objCache.clear();

    DEBUG_PRINT("Cleanup complete.");
}





int main(void) {
    // GLFW and GLEW initialization code (same as before)
    if (!glfwInit()) {
        DEBUG_PRINT("Failed to initialize GLFW");
        return -1;
    }

    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(1920, 1080, "Space Invaders | Trabalho Final", NULL, NULL);
    if (window == NULL) {
        DEBUG_PRINT("Failed to open GLFW window.");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glewExperimental = true;
    if (glewInit() != GLEW_OK) {
        DEBUG_PRINT("Failed to initialize GLEW");
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
    DEBUG_PRINT("Shaders loaded successfully.");

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
    do {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(programID);


        // Calculate deltaTime for smooth movement
        double currentTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;

        computeMatricesFromInputs();
        glm::mat4 ProjectionMatrix = getProjectionMatrix();
        glm::mat4 ViewMatrix = getViewMatrix();


        handlePlayerMovement(playerShip, deltaTime);

        updateAlienPositions(aliens_vector);


        // Render player ship
        playerShip.modelMatrix = glm::translate(glm::mat4(1.0f), playerShip.position);
        renderObject(playerShip, MatrixID, ModelMatrixID, ViewMatrixID, textureID, ProjectionMatrix, ViewMatrix);

        // Render mothership
        motherShip.modelMatrix = glm::translate(glm::mat4(1.0f), motherShip.position);
        renderObject(motherShip, MatrixID, ModelMatrixID, ViewMatrixID, textureID, ProjectionMatrix, ViewMatrix);

        // Render each alien dynamically
        for (auto& alien : aliens_vector) {
            alien.modelMatrix = glm::translate(glm::mat4(1.0f), alien.position);
            renderObject(alien, MatrixID, ModelMatrixID, ViewMatrixID, textureID, ProjectionMatrix, ViewMatrix);
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






