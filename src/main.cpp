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
#include <thread>
#include <atomic>
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

    // 5. Create window (main thread only - GL context gets set up on the render thread)
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

    // 7. Background audio-fill thread
    // fillBuffer used to run in the main loop, but that blocks during window drag/resize,
    // and the ring buffer only holds ~46ms of audio, so it'd glitch. runs on its own thread now.
    std::atomic<bool> audioFileEnded{false};
    std::atomic<bool> stopFillThread{false};
    std::atomic<long long> fillTimeNs{0};
    std::atomic<int> fillCallCount{0};

    std::thread fillThread([&]() {
        while (!stopFillThread.load(std::memory_order_relaxed)) {
            auto fillStart = std::chrono::steady_clock::now();
            bool more = buffer.fillBuffer(bufferSize / 2);
            fillTimeNs.fetch_add(
                std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - fillStart).count(),
                std::memory_order_relaxed);
            fillCallCount.fetch_add(1, std::memory_order_relaxed);

            if (!more) {
                std::cout << "End of file reached, waiting for buffer to drain...\n";
                audioFileEnded.store(true, std::memory_order_relaxed);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    // 8. Render thread
    // rendering used to happen in the main loop, but windows blocks the main thread's
    // message pump during a window drag/resize, which froze the visualization. running
    // it on its own thread means it keeps going no matter what the main thread is doing.
    std::atomic<bool> quitRequested{false};
    std::atomic<double> lastFps{0.0};
    std::atomic<double> lastFrameTimeMs{0.0};
    std::atomic<bool> titleNeedsUpdate{false};

    std::thread renderThread([&]() {
        if (!visualizer.initializeGraphics()) {
            std::cerr << "Error: Could not initialize graphics\n";
            quitRequested.store(true, std::memory_order_relaxed);
            return;
        }

        // fps/frame-time tracking, updated once a second
        int frameCount = 0;
        auto fpsTimerStart = std::chrono::steady_clock::now();

        // per-stage timers (ms), reset every second
        double fftAnalysisMs = 0.0;
        double renderMs = 0.0;

        while (!visualizer.shouldClose() && output.isActive() && !quitRequested.load(std::memory_order_relaxed)) {
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

            // Update fps/frame-time once per second. glfwSetWindowTitle has to run on the
            // main thread, so just stash the numbers here and let main pick them up.
            frameCount++;
            auto now = std::chrono::steady_clock::now();
            std::chrono::duration<double> elapsed = now - fpsTimerStart;
            if (elapsed.count() >= 1.0) {
                double fps = frameCount / elapsed.count();
                lastFps.store(fps, std::memory_order_relaxed);
                lastFrameTimeMs.store((elapsed.count() * 1000.0) / frameCount, std::memory_order_relaxed);
                titleNeedsUpdate.store(true, std::memory_order_relaxed);

                // fill runs on its own thread at its own rate, so average per call not per frame
                int fillCalls = fillCallCount.exchange(0, std::memory_order_relaxed);
                double fillMsAvg = fillCalls > 0
                    ? (fillTimeNs.exchange(0, std::memory_order_relaxed) / 1e6) / fillCalls
                    : 0.0;

                std::cout << "[Perf] FPS: " << fps
                           << " | Avg Fill: " << fillMsAvg << " ms (" << fillCalls << " calls)"
                           << " | Avg FFT: " << (fftAnalysisMs / frameCount) << " ms"
                           << " | Avg Render: " << (renderMs / frameCount) << " ms\n";

                frameCount = 0;
                fpsTimerStart = now;
                fftAnalysisMs = 0.0;
                renderMs = 0.0;
            }

            // Stop if buffer drained
            if (audioFileEnded.load(std::memory_order_relaxed) && buffer.getAvailableReadSamples() == 0) {
                std::cout << "Playback finished.\n";
                output.stop();
                break;
            }
        }

        quitRequested.store(true, std::memory_order_relaxed);
        glfwMakeContextCurrent(nullptr); // release context so main thread can clean up
    });

    // 9. Main thread just pumps window/input events. waitEventsTimeout sleeps
    // efficiently instead of busy-looping, and wakes up periodically to pick up title updates.
    while (!visualizer.shouldClose() && !quitRequested.load(std::memory_order_relaxed)) {
        visualizer.waitEventsTimeout(0.1);

        if (titleNeedsUpdate.exchange(false, std::memory_order_relaxed)) {
            char title[128];
            std::snprintf(title, sizeof(title), "Audio Visualizer - %.1f FPS (%.2f ms/frame)",
                           lastFps.load(std::memory_order_relaxed), lastFrameTimeMs.load(std::memory_order_relaxed));
            visualizer.setWindowTitle(title);
        }
    }

    quitRequested.store(true, std::memory_order_relaxed);
    renderThread.join();

    stopFillThread.store(true, std::memory_order_relaxed);
    fillThread.join();

    output.stop();
    std::cout << "Program finished.\n";
    return 0;
}
