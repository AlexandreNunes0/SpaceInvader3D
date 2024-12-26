// Standard headers
#include <stdio.h>
#include <stdlib.h>
#include <vector>

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
#include <common/objloader.hpp>
#include <common/vboindexer.hpp>
#include <common/texture.hpp>


// Include TinyObjLoader
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

// Function to load OBJ using TinyObjLoader
bool OBJloadingfunction(const char* path, std::vector<glm::vec3>& out_vertices,
	std::vector<glm::vec2>& out_uvs, std::vector<glm::vec3>& out_normals,
	std::vector<tinyobj::material_t>& out_materials) {
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	// Load the OBJ file and its material data
	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path);

	if (!warn.empty()) {
		fprintf(stderr, "TinyObjLoader warning: %s\n", warn.c_str());
	}
	if (!err.empty()) {
		fprintf(stderr, "TinyObjLoader error: %s\n", err.c_str());
	}
	if (!ret) {
		fprintf(stderr, "Failed to load %s\n", path);
		return false;
	}

	// Copy materials to the output (optional, based on your use case)
	out_materials = materials;

	// Parse the loaded data
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
	return true;
}



int main(void) {
	// Initialize GLFW
	if (!glfwInit()) {
		fprintf(stderr, "Failed to initialize GLFW\n");
		getchar();
		return -1;
	}

	// GLFW window settings
	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Create the GLFW window
	window = glfwCreateWindow(1920, 1080, "Star Wars | Trabalho 2", NULL, NULL);
	if (window == NULL) {
		fprintf(stderr, "Failed to open GLFW window.\n");
		getchar();
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// Initialize GLEW
	glewExperimental = true;
	if (glewInit() != GLEW_OK) {
		fprintf(stderr, "Failed to initialize GLEW\n");
		getchar();
		glfwTerminate();
		return -1;
	}

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

	// Get uniform handles
	GLuint MatrixID = glGetUniformLocation(programID, "MVP");
	GLuint ViewMatrixID = glGetUniformLocation(programID, "V");
	GLuint ModelMatrixID = glGetUniformLocation(programID, "M");

	// Load 3D models
	std::vector<glm::vec3> vertices, vertices2, vertices3, vertices4;
	std::vector<glm::vec2> uvs, uvs2, uvs3, uvs4;
	std::vector<glm::vec3> normals, normals2, normals3, normals4;
	std::vector<glm::vec3> texturas, texturas2, texturas3, texturas4;

	

	if (!OBJloadingfunction("obj/alien1.obj", vertices, uvs, normals,texturas)) {
		fprintf(stderr, "Failed to load alien1.obj\n");
		return -1;
	}
	if (!OBJloadingfunction("obj/mothership.obj", vertices3, uvs3, normals3)) {
		fprintf(stderr, "Failed to load mothership.obj\n");
		return -1;
	}
	if (!OBJloadingfunction("obj/player.obj", vertices4, uvs4, normals4)) {
		fprintf(stderr, "Failed to load player.obj\n");
		return -1;
	}



	// Duplicate hangar data
	// so that it doesnt read the hangar file 2 times
	vertices2 = vertices;
	uvs2 = uvs;
	normals2 = normals;




	////////////////////////////////////////////
	//		HANGAR 1 BUFFERS

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


	////////////////////////////////////////////
	//		HANGAR 2 BUFFERS

	GLuint vertexbuffer2, uvbuffer2, normalbuffer2;
	glGenBuffers(1, &vertexbuffer2);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer2);
	glBufferData(GL_ARRAY_BUFFER, vertices2.size() * sizeof(glm::vec3), &vertices2[0], GL_STATIC_DRAW);

	glGenBuffers(1, &uvbuffer2);
	glBindBuffer(GL_ARRAY_BUFFER, uvbuffer2);
	glBufferData(GL_ARRAY_BUFFER, uvs2.size() * sizeof(glm::vec2), &uvs2[0], GL_STATIC_DRAW);

	glGenBuffers(1, &normalbuffer2);
	glBindBuffer(GL_ARRAY_BUFFER, normalbuffer2);
	glBufferData(GL_ARRAY_BUFFER, normals2.size() * sizeof(glm::vec3), &normals2[0], GL_STATIC_DRAW);


	////////////////////////////////////////////
	//		T-65  BUFFERS

	GLuint vertexbuffer3, uvbuffer3, normalbuffer3;
	glGenBuffers(1, &vertexbuffer3);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer3);
	glBufferData(GL_ARRAY_BUFFER, vertices3.size() * sizeof(glm::vec3), &vertices3[0], GL_STATIC_DRAW);

	glGenBuffers(1, &uvbuffer3);
	glBindBuffer(GL_ARRAY_BUFFER, uvbuffer3);
	glBufferData(GL_ARRAY_BUFFER, uvs3.size() * sizeof(glm::vec2), &uvs3[0], GL_STATIC_DRAW);

	glGenBuffers(1, &normalbuffer3);
	glBindBuffer(GL_ARRAY_BUFFER, normalbuffer3);
	glBufferData(GL_ARRAY_BUFFER, normals3.size() * sizeof(glm::vec3), &normals3[0], GL_STATIC_DRAW);


	////////////////////////////////////////////
	//		TAI-FIGHTER BUFFERS

	GLuint vertexbuffer4, uvbuffer4, normalbuffer4;
	glGenBuffers(1, &vertexbuffer4);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer4);
	glBufferData(GL_ARRAY_BUFFER, vertices4.size() * sizeof(glm::vec3), &vertices4[0], GL_STATIC_DRAW);

	glGenBuffers(1, &uvbuffer4);
	glBindBuffer(GL_ARRAY_BUFFER, uvbuffer4);
	glBufferData(GL_ARRAY_BUFFER, uvs4.size() * sizeof(glm::vec2), &uvs4[0], GL_STATIC_DRAW);

	glGenBuffers(1, &normalbuffer4);
	glBindBuffer(GL_ARRAY_BUFFER, normalbuffer4);
	glBufferData(GL_ARRAY_BUFFER, normals4.size() * sizeof(glm::vec3), &normals4[0], GL_STATIC_DRAW);


	////////////////////////////////////////////
	// Light setup in the main code
	// Note: Additional logic for lighting is handled in the shaders

	// Retrieve the uniform location for the light position variable in the shader
	GLuint LightID = glGetUniformLocation(programID, "LightPosition_worldspace");

	// Defines an array of 5 light source positions in the world space
	// Each position is a glm::vec3 representing (x, y, z) coordinates
	glm::vec3 lightPositions[5] = {
		glm::vec3(450.0f, 5.0f, 0.0f),    // Light 1: Positioned in one hangar
		glm::vec3(-450.0f, 5.0f, 0.0f),   // Light 2: Positioned in the other hangar
		glm::vec3(500.0f, 10.0f, -20.0f), // Light 3 
		glm::vec3(-500.0f, 10.0f, 20.0f), // Light 4
		glm::vec3(0.0f, 20.0f, 0.0f)      // Light 5: Centered above the scene
	};

	// Pass the light positions array to the shader as a uniform variable
	// glUniform3fv sends an array of vec3 values (5 in this case) to the shader
	glUniform3fv(glGetUniformLocation(programID, "LightPositions_worldspace"), 5, &lightPositions[0][0]);


	////////////////////////////////////////////
	// Movement parameters for scene objects

	// Uniform scaling factor applied to all objects in the scene to ensure correct sizes
	float uniformScale = 0.1f;

	// Distance between the two hangars
	float hangarDistance = 300.0f;

	// Initial position of the TIE Fighter relative to the hangars
	float tieFighterPosition = -hangarDistance;

	// Speed at which the TIE in the scene moves from one hangar to the other 
	float moving_speed = 2.0f;

	// Rotation angle for the ship when it reaches the end of the hangar
	float shipRotation = 180.0f;

	// Parameters for the TIE Fighter's rotation animation when at the end of the hangars
	float tieFighterRotationProgress = 0.0f; // Tracks the current rotation angle of the TIE Fighter
	float rotationSpeed = 2.0f;              // Speed of rotation
	bool isRotating = false;                 // Flag to indicate if the TIE Fighter is currently rotating
	int direction = 1;                       // Direction of movement or rotation (1 for forward, -1 for backward)

	// Variables for the height adjustment
	float transitionHeight = 50.0f; // Maximum height at the midpoint
	float transitionMidpoint = 0.0f; // Midpoint between the two hangars
	float yPosition = 0.0f; // Default Y position


	////////////////////////////////////////////
	// Main Rendering Loop
	// The core loop of the application, responsible for rendering each frame


	do {
		// Clear the screen by resetting the color and depth buffers
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Activate the shader program to be used for rendering
		glUseProgram(programID);

		// Update the camera and user input-based matrices
		computeMatricesFromInputs();
		glm::mat4 ProjectionMatrix = getProjectionMatrix(); // Get the projection matrix (camera perspective)
		glm::mat4 ViewMatrix = getViewMatrix();             // Get the view matrix (camera position and orientation)
		glm::mat4 ModelMatrix = glm::mat4(1.0f);            // Default model matrix



		////////////////////////////////////////////
		// Rendering Hangar 1

		// Set up transformations for Hangar 1
		glm::mat4 ModelMatrix1 = glm::mat4(1.0f);
		ModelMatrix1 = glm::scale(ModelMatrix1, glm::vec3(uniformScale, uniformScale, uniformScale)); // Apply uniform scaling
		ModelMatrix1 = glm::translate(ModelMatrix1, glm::vec3(-hangarDistance, 0.0f, 0.0f));         // Move Hangar 1 to the left
		glm::mat4 MVP1 = ProjectionMatrix * ViewMatrix * ModelMatrix1; // Calculate Model-View-Projection matrix

		// Send transformation matrices to the shader
		glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP1[0][0]);
		glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix1[0][0]);
		glUniformMatrix4fv(ViewMatrixID, 1, GL_FALSE, &ViewMatrix[0][0]);

		// Render Hangar 1 geometry
		glEnableVertexAttribArray(0); // Enable vertex attribute for position
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
		glEnableVertexAttribArray(1); // Enable vertex attribute for UV coordinates
		glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
		glEnableVertexAttribArray(2); // Enable vertex attribute for normals
		glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
		glDrawArrays(GL_TRIANGLES, 0, vertices.size()); // Draw the vertices as triangles

		// Disable attributes after rendering
		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);



		////////////////////////////////////////////
		// Rendering Hangar 2

		glm::mat4 ModelMatrix2 = glm::mat4(1.0f);
		ModelMatrix2 = glm::scale(ModelMatrix2, glm::vec3(uniformScale, uniformScale, uniformScale));
		ModelMatrix2 = glm::translate(ModelMatrix2, glm::vec3(hangarDistance, 0.0f, 0.0f));
		ModelMatrix2 = glm::rotate(ModelMatrix2, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 MVP2 = ProjectionMatrix * ViewMatrix * ModelMatrix2;

		glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP2[0][0]);
		glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix2[0][0]);
		glUniformMatrix4fv(ViewMatrixID, 1, GL_FALSE, &ViewMatrix[0][0]);

		// Render Hangar 2 geometry
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer2);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, uvbuffer2);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, normalbuffer2);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
		glDrawArrays(GL_TRIANGLES, 0, vertices2.size());

		// Disable attributes after rendering
		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);



		////////////////////////////////////////////
		// Rendering X-Wing (Inside Hangar 1)

		glm::mat4 ModelMatrix3 = glm::mat4(1.0f);
		ModelMatrix3 = glm::scale(ModelMatrix3, glm::vec3(uniformScale, uniformScale, uniformScale));
		ModelMatrix3 = glm::translate(ModelMatrix3, glm::vec3(-hangarDistance, 0.0f, -70.0f));
		ModelMatrix3 = glm::rotate(ModelMatrix3, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 MVP3 = ProjectionMatrix * ViewMatrix * ModelMatrix3;

		glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP3[0][0]);
		glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix3[0][0]);
		glUniformMatrix4fv(ViewMatrixID, 1, GL_FALSE, &ViewMatrix[0][0]);

		// Render X-Wing geometry
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer3);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, uvbuffer3);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, normalbuffer3);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
		glDrawArrays(GL_TRIANGLES, 0, vertices3.size());

		// Disable attributes after rendering
		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);



		//////////////////////////////////////////////////////////////////////////
		//     TIE-FIGHTER RELATED CODE FOR BOTH ANIMATION AND RENDERING        //
		//																        //
		//  This block manages the movement, rotation, and vertical position    //
		// adjustment for the TIE Fighter in the scene. The logic ensures that  //
		// the ship alternates between Hangars 1 and 2, performs rotations      //
		// upon reaching each destination, and includes height adjustments      //
		// during transitions.												    //
		//////////////////////////////////////////////////////////////////////////

// Handling TIE Fighter movement and rotation
		if (!isRotating) {
			// Linear movement towards the target hangar
			if (direction == 1) {
				tieFighterPosition += moving_speed; // Move towards Hangar 2
				if (tieFighterPosition >= hangarDistance) {
					tieFighterPosition = hangarDistance; // Clamp position at Hangar 2
					isRotating = true; // Initiate rotation phase
				}
			}
			else if (direction == -1) {
				tieFighterPosition -= moving_speed; // Move towards Hangar 1
				if (tieFighterPosition <= -hangarDistance) {
					tieFighterPosition = -hangarDistance; // Clamp position at Hangar 1
					isRotating = true; // Initiate rotation phase
				}
			}

			// Adjust Y position for vertical transition during movement
			float distanceFromMidpoint = abs(tieFighterPosition - transitionMidpoint);
			float normalizedDistance = 1.0f - (distanceFromMidpoint / hangarDistance);
			// Ensure the height is highest at the midpoint of the transition
			yPosition = transitionHeight * normalizedDistance;
			// Scale the height by proximity to the midpoint
		}
		else {
			// Rotation phase: Rotate the TIE Fighter upon reaching a hangar
			if (tieFighterRotationProgress < 180.0f) {
				float rotationStep = rotationSpeed;
				tieFighterRotationProgress += rotationStep; // Increment rotation progress
				shipRotation -= rotationStep; // Rotate the ship (direction-dependent)
			}
			else {
				// Reset rotation progress after completing a half-circle rotation
				tieFighterRotationProgress = 0.0f;
				isRotating = false; // Return to movement phase
				direction *= -1; // Reverse direction for next movement phase
			}
		}




		// Rendering TIE Fighter ( depending on current position ) 

		glm::mat4 ModelMatrix4 = glm::mat4(1.0f);
		ModelMatrix4 = glm::scale(ModelMatrix4, glm::vec3(uniformScale, uniformScale, uniformScale));
		ModelMatrix4 = glm::translate(ModelMatrix4, glm::vec3(tieFighterPosition, yPosition, 0.0f)); // Using yPosition for height
		ModelMatrix4 = glm::rotate(ModelMatrix4, glm::radians(shipRotation), glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 MVP4 = ProjectionMatrix * ViewMatrix * ModelMatrix4;

		glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP4[0][0]);
		glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix4[0][0]);
		glUniformMatrix4fv(ViewMatrixID, 1, GL_FALSE, &ViewMatrix[0][0]);

		// Render TIE Fighter geometry
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer4);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, uvbuffer4);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, normalbuffer4);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
		glDrawArrays(GL_TRIANGLES, 0, vertices4.size());

		// Disable attributes after rendering
		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);



		glfwSwapBuffers(window);
		glfwPollEvents();

	} while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS && glfwWindowShouldClose(window) == 0);



	//////////////////////////////////////////////////////////////////
	// Clean up and release all resources before exiting the application

	// Delete buffers associated with the first 3D object (e.g., Hangar 1)
	glDeleteBuffers(1, &vertexbuffer); // Vertex buffer for object positions
	glDeleteBuffers(1, &uvbuffer);    // UV buffer for texture coordinates
	glDeleteBuffers(1, &normalbuffer); // Normal buffer for lighting calculations

	// Delete buffers associated with the second 3D object (e.g., Hangar 2)
	glDeleteBuffers(1, &vertexbuffer2); // Vertex buffer for object positions
	glDeleteBuffers(1, &uvbuffer2);    // UV buffer for texture coordinates
	glDeleteBuffers(1, &normalbuffer2); // Normal buffer for lighting calculations

	// Delete buffers associated with the third 3D object (e.g., X-Wing)
	glDeleteBuffers(1, &vertexbuffer3); // Vertex buffer for object positions
	glDeleteBuffers(1, &uvbuffer3);    // UV buffer for texture coordinates
	glDeleteBuffers(1, &normalbuffer3); // Normal buffer for lighting calculations

	// Delete buffers associated with the fourth 3D object (e.g., TIE Fighter)
	glDeleteBuffers(1, &vertexbuffer4); // Vertex buffer for object positions
	glDeleteBuffers(1, &uvbuffer4);    // UV buffer for texture coordinates
	glDeleteBuffers(1, &normalbuffer4); // Normal buffer for lighting calculations

	// Delete the Vertex Array Object (VAO), which holds vertex configuration states
	glDeleteVertexArrays(1, &VertexArrayID);

	// Delete the shader program used for rendering
	glDeleteProgram(programID);

	// Terminate GLFW to clean up windowing and context-related resources
	glfwTerminate();

	// Return 0 to indicate successful program termination
	return 0;

}


