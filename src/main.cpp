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
#include <chrono>
#include <cstdio>
#include <tinyfiledialogs.h>

#include <fftw3.h>

int main() {
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

    // FPS/frame-time tracking (measured over the real loop, sleep included)
    int frameCount = 0;
    auto fpsTimerStart = std::chrono::steady_clock::now();

    // Per-stage timing breakdown, accumulated over each 1-second window (milliseconds)
    double audioFillMs = 0.0;
    double fftAnalysisMs = 0.0;
    double renderMs = 0.0;

    while (!visualizer.shouldClose() && output.isActive()) {
        // Fill buffer if needed
        auto fillStart = std::chrono::steady_clock::now();
        if (!fileEnded) {
            if (!buffer.fillBuffer(bufferSize / 2)) {
                std::cout << "End of file reached, waiting for buffer to drain...\n";
                fileEnded = true;
            }
        }
        audioFillMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - fillStart).count();

        // Analyze audio and update visualizer
        auto fftStart = std::chrono::steady_clock::now();
        bool hasNewAnalysis = analyzer.analyzeNextBlock();
        fftAnalysisMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - fftStart).count();
        if (hasNewAnalysis) {
            visualizer.updateData(analyzer.getBuckets());
        }

        // Render visualization
        auto renderStart = std::chrono::steady_clock::now();
        visualizer.render();
        renderMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - renderStart).count();
        visualizer.pollEvents();

        // Update live FPS / frame-time display once per second
        frameCount++;
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = now - fpsTimerStart;
        if (elapsed.count() >= 1.0) {
            double fps = frameCount / elapsed.count();
            double frameTimeMs = (elapsed.count() * 1000.0) / frameCount;
            char title[128];
            std::snprintf(title, sizeof(title), "Audio Visualizer - %.1f FPS (%.2f ms/frame)", fps, frameTimeMs);
            visualizer.setWindowTitle(title);

            // Per-stage breakdown (averaged per frame over the last second)
            std::cout << "[Perf] FPS: " << fps
                       << " | Avg Fill: " << (audioFillMs / frameCount) << " ms"
                       << " | Avg FFT: " << (fftAnalysisMs / frameCount) << " ms"
                       << " | Avg Render: " << (renderMs / frameCount) << " ms\n";

            frameCount = 0;
            fpsTimerStart = now;
            audioFillMs = 0.0;
            fftAnalysisMs = 0.0;
            renderMs = 0.0;
        }
        
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