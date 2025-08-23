#include <iostream>
#include <GLFW/glfw3.h>
#include <portaudio.h>
#include <sndfile.h>
#include <vector>
#include "AudioLoader.h"
#include "AudioBuffer.h"
#include "AudioOutput.h"

#include <thread>
#include <chrono>

#include <fftw3.h>

int main() {
    
    // 1. Load audio file
    AudioLoader loader;
    if (!loader.loadAudioFile("../assets/delete.mp3")) {
        std::cerr << "Error: Could not load test.wav\n";
        return 1;
    }

    std::cout << "Loaded file: "
              << loader.getSampleRate() << " Hz, "
              << loader.getChannels() << " channels, "
              << loader.getDuration() << " seconds\n";

    // 2. Create buffer
    const int bufferSize = 8192; // samples
    AudioBuffer buffer(bufferSize, loader);

    // 3. Pre-fill buffer before starting playback
    while (buffer.fillBuffer(bufferSize / 2)) {
        if (buffer.getAvailableReadSamples() >= bufferSize / 2) break;
    }

    // 4. Create audio output
    AudioOutput output(&buffer, loader.getSampleRate(), loader.getChannels());

    // 5. Start playback
    if (!output.start()) {
        std::cerr << "Error: Could not start audio output\n";
        return 1;
    }

    std::cout << "Playing audio...\n";

    // 6. Keep filling buffer while audio plays
    while (output.isActive()) {
        if (!buffer.fillBuffer(bufferSize / 2)) {
            // No more data to load, let playback finish
            std::cout << "End of file reached, waiting for playback to finish...\n";
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // 7. Wait until stream finishes naturally
    while (output.isActive()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    output.stop();
    std::cout << "Playback finished.\n";
    return 0;
    /*AudioLoader loader;

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
    */
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