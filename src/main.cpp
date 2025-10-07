#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <portaudio.h>
#include <sndfile.h>
#include <vector>
#include "AudioLoader.h"
#include "AudioBuffer.h"
#include "AudioOutput.h"
#include "AudioAnalyzer.h"
#include <cmath>
#include <thread>
#include <chrono>

#include <fftw3.h>
    //PUT IN VISUALIZER
void framebuffer_size_callback(GLFWwindow* window, int width, int height){
    glViewport(0, 0, width, height);
}  
void processInput(GLFWwindow *window){
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

int main() {
    int choice;
    std::cout << "1 for audio stuff 2 for glfw testing";
    std::cin >> choice;
    if(choice == 1){
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

        // 4. Create audio output and audio analyzer
        AudioOutput output(&buffer, loader.getSampleRate(), loader.getChannels());
        AudioAnalyzer analyzer(&buffer, 1024, loader.getSampleRate());
        // 5. Start playback
        if (!output.start()) {
            std::cerr << "Error: Could not start audio output\n";
            return 1;
        }

        std::cout << "Playing audio...\n";

            // 6. Keep filling buffer while audio plays
        bool fileEnded = false;

        while (output.isActive()) {
            if (!fileEnded) {
                if (!buffer.fillBuffer(bufferSize / 2)) {
                    // No more data left to load from file
                    std::cout << "End of file reached, waiting for buffer to drain...\n";
                    fileEnded = true;
                }
            }

            analyzer.analyzeNextBlock();
            // analyzer test, but causes delay
            /*if (analyzer.analyzeNextBlock()) {
                std::cout << "RMS: " << analyzer.getRmsVal()
                        << "  Peak: " << analyzer.getPeakAmplitude()
                        << "\nBuckets: ";
                for (float b : analyzer.getBuckets()) {
                    std::cout << b << " ";
                }
                std::cout << "\n";
            }*/

            std::this_thread::sleep_for(std::chrono::milliseconds(20));

            // If file is ended AND buffer is drained, stop playback
            if (fileEnded && buffer.getAvailableReadSamples() == 0) {
                std::cout << "Playback finished.\n";
                output.stop();
                break;
            }
        }

        // 7. Wait until stream finishes naturally
        while (output.isActive()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        output.stop();
        std::cout << "Playback finished.\n";
        return 0;
        //testing just loader
        AudioLoader loader1;

        if (loader1.loadAudioFile("../assets/testaudio.mp3")) {
            std::cout << "Loaded file successfully.\n";
            std::cout << "Sample rate: " << loader1.getSampleRate() << "\n";
            std::cout << "Channels: " << loader1.getChannels() << "\n";
            std::cout << "Frames: " << loader1.getTotalFrames() << "\n";
            std::cout << "Duration: " << loader1.getDuration() << " seconds\n";
            std::cout << "First sample value: " << loader1.getAudioData()[0] << "\n";
        } else {
            std::cout << "Failed to load file.\n";
        }
        return 0;
        
    }
    else{
        
        // initialize GLFW
        if (!glfwInit()) {
            std::cerr << "GLFW failed to initialize\n";
            return false;
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        // Create a window
        GLFWwindow* window = glfwCreateWindow(800, 600, "Audio Visualizer", nullptr, nullptr);
        if (!window) {
            std::cout << "Failed to create GLFW window\n";
            glfwTerminate();
            return -1;
        }
        glfwMakeContextCurrent(window);
        glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
        // Initialize GLAD
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            std::cout << "Failed to initialize GLAD" << std::endl;
            return -1;
        }    

        glViewport(0, 0, 800, 600);
        
        while(!glfwWindowShouldClose(window)){
            //input
            processInput(window);
            //rendering commands
            glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            //...
            // check and call events and swap the buffers
            glfwSwapBuffers(window);
            glfwPollEvents();    
        }
        glfwTerminate();

        return 0;
    }
}