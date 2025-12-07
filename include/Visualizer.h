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
		// Sets up shaders for bar rendering
		bool setupShaders();
		// Sets up vertex buffer for rendering bars
		bool setupGeometry();
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
		void updateData();
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

Visualizer::~Visualizer(){
	
}
bool Visualizer::setupWindow(){
	
}
bool Visualizer::setupShaders(){
	
}
bool Visualizer::setupGeometry(){
	
}
static void framebufferSizeCallback(GLFWwindow* window, int width, int height){
	glViewport(0, 0, width, height);
	//update stored dimensions? 
}

bool Visualizer::initialize(){
	// initialize GLFW
    if (!glfwInit()) {
		std::cerr << "GLFW failed to initialize\n";
		return false;
	}
	setupWindow;
}