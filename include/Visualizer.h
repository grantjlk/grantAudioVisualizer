#ifndef VISUALIZER_H
#define VISUALIZER_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <iostream>

class Visualizer{
	private:
		GLFWwindow* window;
		int windowWidth;
    	int windowHeight;

		// Opengl objects
		GLuint shaderProgram;
		GLuint VAO;
		GLuint VBO;
		 // Shader uniforms
		GLint uniformBarHeights;
		GLint uniformBarCount;
		GLint uniformBarColor;
		// Bar data
		int numBars;
		std::vector<float> barHeights;
		// Sets up GLFW window and OpenGL context
		bool setupWindow();
		// compiiles shader from source code
		GLuint _compileShader(const char* source, GLenum type);
		// Links vertex and fragment shaders into a program
		GLuint _createShaderProgram(GLuint vertexShader, GLuint fragmentShader);
		// Sets up shaders for bar rendering
		bool setupShaders();
		// Sets up vertex buffer for rendering bars
		void setupGeometry();
		// GLFW callback for window resizing
		static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
	public:
		//initializes window dimensions and bar count
		Visualizer(int width = 800, int height = 600, int numBars = 32);
		// cleans up opengl resources and glfw
		~Visualizer();
		// initializes GLFW, creates window, and sets up OpenGL context
		bool initialize();
		// updates bar heights from analyzer data
		void updateData(const std::vector<float>& buckets);
		// Renders the frequency bars to the window
		void render();
		// Checks if window should close 
		bool shouldClose() const;
		// polls for input
		void pollEvents();
		//? maybe void setBarColor(float r, float g, float b)
		//TODO	add smoothing if needed in future??
		// Disable copy constructor and assignment operator
    	Visualizer(const Visualizer&) = delete;
    	Visualizer& operator=(const Visualizer&) = delete;
};
#endif