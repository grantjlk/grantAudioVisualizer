#include <iostream>
#include <GLFW/glfw3.h>
#include <portaudio.h>
#include <sndfile.h>


int main() {
   // initialize GLFW
    if (!glfwInit()) {
        std::cerr << "GLFW failed to initialize\n";
        return -1;
    }
    
    // Create a window
    GLFWwindow* window = glfwCreateWindow(800, 600, "Audio Visualizer", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    
	//make context current
    glfwMakeContextCurrent(window);
    
    while (!glfwWindowShouldClose(window)) {
		// poll for events
        glfwPollEvents();
        
        // Check if esc key was pressed
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }
        
    }
    
    // clean up window and terminate GLFW
    glfwDestroyWindow(window);
    glfwTerminate();
    
    std::cout << "Window closed cleanly.\n";
    return 0;
}