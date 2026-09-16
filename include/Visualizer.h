#ifndef VISUALIZER_H
#define VISUALIZER_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <iostream>
#include <string>
#include <atomic>

class Visualizer{
	private:
		GLFWwindow* window;
		std::atomic<int> windowWidth;
    	std::atomic<int> windowHeight;

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
		// smoothing info
		std::vector<float> smoothedHeights;
		float smoothingFactor;
		// Creates the GLFW window (main thread only, no GL calls)
		bool setupWindow();
		// compiiles shader from source code
		GLuint _compileShader(const char* source, GLenum type);
		// Links vertex and fragment shaders into a program
		GLuint _createShaderProgram(GLuint vertexShader, GLuint fragmentShader);
		// Sets up shaders for bar rendering
		bool setupShaders();
		// Sets up vertex buffer for rendering bars
		void setupGeometry();
		// GLFW callback for window resizing - just stores the new size, no GL calls
		// (render() applies it via glViewport on the render thread each frame)
		static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
	public:
		//initializes window dimensions and bar count
		Visualizer(int width = 800, int height = 600, int numBars = 32);
		// cleans up opengl resources and glfw
		~Visualizer();
		// creates the window (call from main thread)
		bool initialize();
		// makes the GL context current on the calling thread, loads GLAD, compiles
		// shaders, sets up geometry - call once from whichever thread will render
		bool initializeGraphics();
		// updates bar heights from analyzer data
		void updateData(const std::vector<float>& buckets);
		// Renders the frequency bars to the window
		void render();
		// Checks if window should close
		bool shouldClose() const;
		// polls for input
		void pollEvents();
		// like pollEvents but sleeps efficiently up to timeoutSeconds if nothing happens
		void waitEventsTimeout(double timeoutSeconds);

		void setSmoothingFactor(float factor);
			// Updates the window title (used to display live FPS/frame time)
			void setWindowTitle(const std::string& title);
		//? maybe void setBarColor(float r, float g, float b)
		//TODO	add smoothing if needed in future??
		// Disable copy constructor and assignment operator
    	Visualizer(const Visualizer&) = delete;
    	Visualizer& operator=(const Visualizer&) = delete;
};
#endif
