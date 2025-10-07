#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

class Visualizer{
	private:
		void setupWindow();

	public:
		~Visualizer();
		bool initialize();
		//bool updateData();
		//bool render();
};

Visualizer::~Visualizer(){
	
}
void Visualizer::setupWindow(){
	
}

bool Visualizer::initialize(){
	// initialize GLFW
    if (!glfwInit()) {
		std::cerr << "GLFW failed to initialize\n";
		return false;
	}
}