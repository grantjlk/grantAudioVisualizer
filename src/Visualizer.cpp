#include "Visualizer.h"

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