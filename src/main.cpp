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
#include "Visualizer.h"
#include <cmath>
#include <thread>
#include <chrono>
#include <tinyfiledialogs.h>

#include <fftw3.h>

int main() {
/*commented out for now, testing main with visuals
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
/*
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
*/
// 0. File dialog popup to select file
    const char* filterPatterns[] = { "*.mp3", "*.wav", "*.flac"};
    char const * selection = tinyfd_openFileDialog(
        "Select Audio File", // title
        "", // optional initial directory
        2, // number of filter patterns
        filterPatterns, // char const * lFilterPatterns[2] = { "*.txt", "*.jpg" };
        "Audio Files (*.mp3, *.wav, *.flac)", // optional filter description
        0 // forbids multiple selections
        ); 

    if (!selection) {
        std::cerr << "No file selected\n";
        return 1;
    }
// 1. Load audio file
    AudioLoader loader;
    if (!loader.loadAudioFile(selection)) {
        std::cerr << "Error: Could not load audio file\n";
        return 1;
    }

    std::cout << "Loaded file: "
              << loader.getSampleRate() << " Hz, "
              << loader.getChannels() << " channels, "
              << loader.getDuration() << " seconds\n";

    // 2. Create buffer
    const int bufferSize = 8192;
    AudioBuffer buffer(bufferSize, loader);

    // 3. Pre-fill buffer
    while (buffer.fillBuffer(bufferSize / 2)) {
        if (buffer.getAvailableReadSamples() >= bufferSize / 2) break;
    }

    // 4. Create audio output and analyzer
    AudioOutput output(&buffer, loader.getSampleRate(), loader.getChannels());
    AudioAnalyzer analyzer(&buffer, 1024, loader.getSampleRate());
    
    // 5. Create visualizer
    Visualizer visualizer(800, 600, 32);
    if (!visualizer.initialize()) {
        std::cerr << "Error: Could not initialize visualizer\n";
        return 1;
    }
    
    // optional: Adjust smoothing (0.0 = no smoothing, 0.9 = very smooth)
    visualizer.setSmoothingFactor(0.6f);
   

    // 6. Start playback
    if (!output.start()) {
        std::cerr << "Error: Could not start audio output\n";
        return 1;
    }

    std::cout << "Playing audio with visualization...\n";

    // 7. Main loop
    bool fileEnded = false;
    
    while (!visualizer.shouldClose() && output.isActive()) {
        // Fill buffer if needed
        if (!fileEnded) {
            if (!buffer.fillBuffer(bufferSize / 2)) {
                std::cout << "End of file reached, waiting for buffer to drain...\n";
                fileEnded = true;
            }
        }
        
        // Analyze audio and update visualizer
        if (analyzer.analyzeNextBlock()) {
            visualizer.updateData(analyzer.getBuckets());
        }
        
        // Render visualization
        visualizer.render();
        visualizer.pollEvents();
        
        // Small delay to prevent maxing out CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS (5 is smoother)
        
        // Stop if buffer drained
        if (fileEnded && buffer.getAvailableReadSamples() == 0) {
            std::cout << "Playback finished.\n";
            output.stop();
            break;
        }
    }

    output.stop();
    std::cout << "Program finished.\n";
    return 0;
}