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

// Function to load textures and cache them
GLuint loadTexture(const std::string& texturePath) {
    // Check if the texture is already loaded
    if (textureCache.find(texturePath) != textureCache.end()) {
        std::cout << "Texture already loaded: " << texturePath << std::endl;
        return textureCache[texturePath];
    }

    int width, height, channels;
    std::cout << "Attempting to load texture from: " << texturePath << std::endl;
    unsigned char* data = stbi_load(texturePath.c_str(), &width, &height, &channels, 0);
    if (!data) {
        std::cerr << "Failed to load texture: " << texturePath << std::endl;
        std::cerr << "stbi_error: " << stbi_failure_reason() << std::endl;
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

    std::cout << "Successfully loaded texture: " << texturePath << std::endl;
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

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    std::cout << "Loading OBJ file: " << objpath << std::endl;
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
            std::cout << "Found texture: " << fullTexturePath << std::endl;
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

    std::cout << "OBJ file loaded successfully.\n";
    return true;
}

// Function to load game object data and initialize buffers
void loadGameObject(GameObject& obj) {
    std::cout << "Loading GameObject: " << obj.objFile << std::endl;
    if (!OBJloadingfunction(obj.objFile.c_str(), obj.mtlFile.c_str(), obj.vertices, obj.uvs, obj.normals, obj.materials, obj.textures, obj.textureIDs)) {
        std::cerr << "Failed to load GameObject: " << obj.objFile << std::endl;
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

    std::cout << "GameObject buffers created successfully.\n";
}

// Main loop to render objects
int main(void) {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(1920, 1080, "Space Invaders | Trabalho Final", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Failed to open GLFW window.\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glewExperimental = true;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW\n";
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
    std::cout << "Shaders loaded successfully.\n";

    GLuint MatrixID = glGetUniformLocation(programID, "MVP");
    GLuint ViewMatrixID = glGetUniformLocation(programID, "V");
    GLuint ModelMatrixID = glGetUniformLocation(programID, "M");
    GLuint textureID = glGetUniformLocation(programID, "textureSampler");

    std::vector<GameObject> aliens;

    // Create aliens dynamically
    const float spacing = 5.0f;
    const glm::vec3 startPos(-6.0f, 4.0f, 0.0f);

    std::cout << "Creating aliens...\n";
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
            aliens.push_back(alien);
        }
    }

    std::cout << "Aliens created: " << aliens.size() << std::endl;

    // Main render loop
    do {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(programID);

        computeMatricesFromInputs();
        glm::mat4 ProjectionMatrix = getProjectionMatrix();
        glm::mat4 ViewMatrix = getViewMatrix();

        for (auto& alien : aliens) {
            alien.modelMatrix = glm::translate(glm::mat4(1.0f), alien.position);
            glm::mat4 MVP = ProjectionMatrix * ViewMatrix * alien.modelMatrix;

            glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &alien.modelMatrix[0][0]);
            glUniformMatrix4fv(ViewMatrixID, 1, GL_FALSE, &ViewMatrix[0][0]);

            for (size_t i = 0; i < alien.textures.size(); i++) {
                if (alien.textureIDs[i] != 0) {
                    glActiveTexture(GL_TEXTURE0 + i);
                    glBindTexture(GL_TEXTURE_2D, alien.textureIDs[i]);
                    glUniform1i(textureID, i);
                }
            }

            // Bind and draw the object
            glBindVertexArray(alien.vertexArrayID);
            glEnableVertexAttribArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, alien.vertexBuffer);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

            glEnableVertexAttribArray(1);
            glBindBuffer(GL_ARRAY_BUFFER, alien.uvBuffer);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

            glEnableVertexAttribArray(2);
            glBindBuffer(GL_ARRAY_BUFFER, alien.normalBuffer);
            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

            glDrawArrays(GL_TRIANGLES, 0, alien.vertices.size());

            glDisableVertexAttribArray(0);
            glDisableVertexAttribArray(1);
            glDisableVertexAttribArray(2);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    } while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS && glfwWindowShouldClose(window) == 0);

    std::cout << "Exiting program...\n";
    glfwTerminate();
    return 0;
}


