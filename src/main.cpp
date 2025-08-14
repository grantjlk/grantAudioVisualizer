#include <iostream>
#include <GLFW/glfw3.h>
#include <portaudio.h>
#include <sndfile.h>
#include <vector>
#include "AudioLoader.h"

// portaudio callback function, this gets called when audio needs to be played
static int patestCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData )
{
    float *out = (float*)outputBuffer;
    // For now, we just return paContinue to keep the stream open
    return paContinue;
}



int main() {
    AudioLoader loader;

    if (loader.loadAudioFile("../assets/delete.mp3")) {
        std::cout << "Loaded file successfully.\n";
        std::cout << "Sample rate: " << loader.getSampleRate() << "\n";
        std::cout << "Channels: " << loader.getChannels() << "\n";
        std::cout << "Frames: " << loader.getTotalFrames() << "\n";
        std::cout << "Duration: " << loader.getDuration() << " seconds\n";
        std::cout << "First sample value: " << loader.getAudioData()[0] << "\n";
    } else {
        std::cout << "Failed to load file.\n";
    }
    return 0;
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
    //enable vsync
    glfwSwapInterval(1);
    
    while (!glfwWindowShouldClose(window)) {
        // Clear the screen with a dark color
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // TODO: have my visualizer code here

        // Swap buffers and poll for input
        glfwSwapBuffers(window);
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

    return 0;
}