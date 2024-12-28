// Standard headers
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <iostream>


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

GLuint loadTexture(const std::string& texturePath) {
    // Load image using stb_image
    int width, height, channels;
    std::cout << "Attempting to load texture from: " << texturePath << std::endl; // Debug print
    unsigned char* data = stbi_load(texturePath.c_str(), &width, &height, &channels, 0);
    if (!data) {
        std::cerr << "Failed to load texture: " << texturePath << std::endl;
        std::cerr << "stbi_error: " << stbi_failure_reason() << std::endl;  // Print the error reason
        return 0;
    }

    // Generate and bind a new texture
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Load the image into OpenGL texture
    if (channels == 3) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    }
    else if (channels == 4) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    }

    // Generate mipmaps for better texture filtering
    glGenerateMipmap(GL_TEXTURE_2D);

    // Free the image memory after loading
    stbi_image_free(data);

    std::cout << "Successfully loaded texture: " << texturePath << std::endl;
    return textureID;
}


bool OBJloadingfunction(const char* objpath, const char* mtlpath,
    std::vector<glm::vec3>& out_vertices,
    std::vector<glm::vec2>& out_uvs,
    std::vector<glm::vec3>& out_normals,
    std::vector<tinyobj::material_t>& out_materials,
    std::vector<std::string>& out_textures,
    std::vector<GLuint>& out_textureIDs) {  // New vector for texture IDs

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    // Load the OBJ and MTL data
    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, objpath, mtlpath);

    // Check for errors
    if (!warn.empty()) {
        std::cerr << "Warning: " << warn << std::endl;
    }
    if (!err.empty()) {
        std::cerr << "Error: " << err << std::endl;
        return false;
    }

    std::cout << "Successfully loaded OBJ file: " << objpath << std::endl;

    // Extract materials and store texture paths
    out_materials = materials;
    std::cout << "Extracted " << materials.size() << " materials from MTL file." << std::endl;

    // Construct the full texture path by combining objpath and texture name
    std::string mtlFolderPath = std::string(mtlpath);  // The folder of the MTL file
    size_t lastSlash = mtlFolderPath.find_last_of('/');
    if (lastSlash != std::string::npos) {
        mtlFolderPath = mtlFolderPath.substr(0, lastSlash);  // Get the directory of the MTL file
    }

    for (const auto& material : materials) {
        if (!material.diffuse_texname.empty()) {
            // If the material has a diffuse texture, store the full texture path
            std::string fullTexturePath = mtlFolderPath + "/" + material.diffuse_texname;
            out_textures.push_back(fullTexturePath);  // Full path to texture
            std::cout << "Found texture: " << fullTexturePath << std::endl;
        }
        else {
            out_textures.push_back("");  // No texture for this material
            std::cout << "No texture for material." << std::endl;
        }
    }

    // Load textures from the stored texture paths
    for (const auto& texturePath : out_textures) {
        if (!texturePath.empty()) {
            // If texture exists, load it
            GLuint textureID = loadTexture(texturePath);
            out_textureIDs.push_back(textureID);
        }
        else {
            // No texture, so push 0
            out_textureIDs.push_back(0);
        }
    }


    std::cout << "Extracting vertex data..." << std::endl;
    // Extract the vertex data as before
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            glm::vec3 vertex;
            glm::vec2 uv;
            glm::vec3 normal;

            // Vertices
            vertex.x = attrib.vertices[3 * index.vertex_index + 0];
            vertex.y = attrib.vertices[3 * index.vertex_index + 1];
            vertex.z = attrib.vertices[3 * index.vertex_index + 2];
            out_vertices.push_back(vertex);

            // Normals
            if (index.normal_index >= 0) {
                normal.x = attrib.normals[3 * index.normal_index + 0];
                normal.y = attrib.normals[3 * index.normal_index + 1];
                normal.z = attrib.normals[3 * index.normal_index + 2];
                out_normals.push_back(normal);
            }

            // Texture coordinates
            if (index.texcoord_index >= 0) {
                uv.x = attrib.texcoords[2 * index.texcoord_index + 0];
                uv.y = attrib.texcoords[2 * index.texcoord_index + 1];
                out_uvs.push_back(uv);
            }
        }
    }

    std::cout << "Finished extracting vertices, uvs, and normals." << std::endl;

    return true;
}






int main(void) {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    std::cout << "GLFW initialized successfully." << std::endl;


    // GLFW window settings
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create the GLFW window
    window = glfwCreateWindow(1920, 1080, "Space Invaders | Trabalho Final", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Failed to open GLFW window.\n";
        glfwTerminate();
        return -1;
    }

    std::cout << "GLFW window created successfully." << std::endl;

    glfwMakeContextCurrent(window);

    // Initialize GLEW
    glewExperimental = true;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW\n";
        glfwTerminate();
        return -1;
    }

    std::cout << "GLEW initialized successfully." << std::endl;

    // Configure input modes
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPos(window, 1920 / 2, 1080 / 2);

    // OpenGL settings
    glClearColor(0.4f, 0.4f, 0.4f, 0.0f); // Background color

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);

    // Create Vertex Array Object
    GLuint VertexArrayID;
    glGenVertexArrays(1, &VertexArrayID);
    glBindVertexArray(VertexArrayID);

    // Load shaders
    GLuint programID = LoadShaders("TransformVertexShader.vertexshader", "TextureFragmentShader.fragmentshader");
    if (programID == 0) {
        std::cerr << "Shader program failed to load.\n";
        return -1;
    }
    std::cout << "Shaders loaded successfully." << std::endl;


    // Get uniform handles
    GLuint MatrixID = glGetUniformLocation(programID, "MVP");
    GLuint ViewMatrixID = glGetUniformLocation(programID, "V");
    GLuint ModelMatrixID = glGetUniformLocation(programID, "M");
    GLuint textureID = glGetUniformLocation(programID, "textureSampler");  // The texture uniform in the fragment shader

    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec3> normals;
    std::vector<tinyobj::material_t> materials;
    std::vector<std::string> textures;
    std::vector<GLuint> textureIDs;  // Store texture IDs for each material

    if (!OBJloadingfunction("obj/player.obj", "obj", vertices, uvs, normals, materials, textures, textureIDs)) {
        fprintf(stderr, "Failed to load OBJ file\n");
        return -1;
    }
    else {
        std::cout << "OBJ file loaded successfully!" << std::endl;
    }



    // Create buffers
    GLuint vertexbuffer, uvbuffer, normalbuffer;
    glGenBuffers(1, &vertexbuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), &vertices[0], GL_STATIC_DRAW);

    glGenBuffers(1, &uvbuffer);
    glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
    glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(glm::vec2), &uvs[0], GL_STATIC_DRAW);

    glGenBuffers(1, &normalbuffer);
    glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), &normals[0], GL_STATIC_DRAW);

    // Set light positions (not changing, but should be passed to shader as before)
    glm::vec3 lightPositions[5] = {
        glm::vec3(5.0f, 0.0f, 0.0f),    // Light 1
        glm::vec3(-5.0f, 5.0f, 0.0f),   // Light 2
        glm::vec3(2.0f, 0.0f, 0.0f),    // Light 3
        glm::vec3(1.0f, 0.0f, 0.0f),    // Light 4
        glm::vec3(0.0f, 0.0f, 0.0f)     // Light 5
    };

    glUniform3fv(glGetUniformLocation(programID, "LightPositions_worldspace"), 5, &lightPositions[0][0]);

    // Start the main loop
    float uniformScale = 1.0f;

    do {
        // Clear the screen by resetting the color and depth buffers
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Activate the shader program to be used for rendering
        glUseProgram(programID);

        // Update the camera and user input-based matrices
        computeMatricesFromInputs();
        glm::mat4 ProjectionMatrix = getProjectionMatrix();
        glm::mat4 ViewMatrix = getViewMatrix();
        glm::mat4 ModelMatrix = glm::mat4(1.0f);  // Default model matrix

        glm::mat4 MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;

        // Send matrices to the shader
        glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);
        glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
        glUniformMatrix4fv(ViewMatrixID, 1, GL_FALSE, &ViewMatrix[0][0]);

        // Loop through each object and bind its texture before rendering
        for (size_t i = 0; i < textureIDs.size(); ++i) {
            GLuint currentTexture = textureIDs[i]; // Get the texture ID for the current material
            if (currentTexture != 0) {
                glActiveTexture(GL_TEXTURE0);  // Activate texture unit 0
                glBindTexture(GL_TEXTURE_2D, currentTexture);  // Bind the texture to the shader
                glUniform1i(textureID, 0);  // Tell the shader to use texture unit 0
            }

            // Render the object (e.g., Hangar 1) with the bound texture
            glEnableVertexAttribArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

            glEnableVertexAttribArray(1);
            glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

            glEnableVertexAttribArray(2);
            glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

            glDrawArrays(GL_TRIANGLES, 0, vertices.size());

            // Disable vertex attributes after rendering
            glDisableVertexAttribArray(0);
            glDisableVertexAttribArray(1);
            glDisableVertexAttribArray(2);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();

    } while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS && glfwWindowShouldClose(window) == 0);


    std::cout << "Exiting main loop." << std::endl;

    // Cleanup
    glDeleteBuffers(1, &vertexbuffer);
    glDeleteBuffers(1, &uvbuffer);
    glDeleteBuffers(1, &normalbuffer);
    glDeleteVertexArrays(1, &VertexArrayID);
    glDeleteProgram(programID);
    glfwTerminate();

    std::cout << "Resources cleaned up and program terminated." << std::endl;

    return 0;
}



