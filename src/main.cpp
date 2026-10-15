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
#include <algorithm>
#include <iomanip>
#include <tinyfiledialogs.h>

#include <fftw3.h>

// nearest-rank percentile, expects sorted input. p is 0-100 (eg. 99 for p99)
static double percentile(const std::vector<double>& sorted, double p) {
    size_t rank = static_cast<size_t>(std::ceil(p / 100.0 * sorted.size()));
    rank = std::clamp<size_t>(rank, 1, sorted.size());
    return sorted[rank - 1];
}

int main(int argc, char* argv[]) {
// 0. Use the file path from the command line if given (so perf runs can use the same file
// every time), otherwise file dialog popup to select file
    char const * selection = nullptr;
    if (argc > 1) {
        selection = argv[1];
    } else {
        const char* filterPatterns[] = { "*.mp3", "*.wav", "*.flac"};
        selection = tinyfd_openFileDialog(
            "Select Audio File", // title
            "", // optional initial directory
            3, // number of filter patterns
            filterPatterns, // char const * lFilterPatterns[2] = { "*.txt", "*.jpg" };
            "Audio Files (*.mp3, *.wav, *.flac)", // optional filter description
            0 // forbids multiple selections
            );
    }

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
    // underrun count when the file ran out, so the expected one at the very end (buffer
    // draining its last partial chunk) doesn't show up in the final summary
    std::atomic<int> underrunsAtEof{0};

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
                underrunsAtEof.store(output.getUnderrunCount(), std::memory_order_relaxed);
                // release so underrunsAtEof is guaranteed visible to whoever sees audioFileEnded == true
                audioFileEnded.store(true, std::memory_order_release);
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

        // ms elapsed since a time point, just to keep the timing lines short
        auto msSince = [](std::chrono::steady_clock::time_point start) {
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        };

        // per-second stats, reset every second
        int frameCount = 0;
        auto statsTimerStart = std::chrono::steady_clock::now();
        double fftMs = 0.0;
        double drawMs = 0.0;
        double swapMs = 0.0;
        double worstFrameMs = 0.0;

        // every frame time for the whole run, for percentiles at the end. averages hide stutters,
        // percentiles show them. reserved for ~10 min at 60fps so it doesn't reallocate mid run
        std::vector<double> allFrameTimesMs;
        allFrameTimesMs.reserve(60 * 60 * 10);
        auto lastFrameStart = std::chrono::steady_clock::now();
        bool firstFrame = true;

        std::cout << std::fixed << std::setprecision(3);

        while (!visualizer.shouldClose() && output.isActive() && !quitRequested.load(std::memory_order_relaxed)) {
            // frame time = start of last frame to start of this one, so it includes everything (vsync wait too)
            auto frameStart = std::chrono::steady_clock::now();
            if (!firstFrame) {
                double frameMs = std::chrono::duration<double, std::milli>(frameStart - lastFrameStart).count();
                allFrameTimesMs.push_back(frameMs);
                worstFrameMs = std::max(worstFrameMs, frameMs);
            }
            firstFrame = false;
            lastFrameStart = frameStart;

            // Analyze audio and update visualizer
            bool hasNewAnalysis = analyzer.analyzeNextBlock();
            fftMs += msSince(frameStart);
            if (hasNewAnalysis) {
                visualizer.updateData(analyzer.getBuckets());
            }

            // Draw (cpu side of issuing the gl calls, the actual work)
            auto drawStart = std::chrono::steady_clock::now();
            visualizer.render();
            drawMs += msSince(drawStart);

            // Swap, mostly just waiting for the next monitor refresh because of vsync
            auto swapStart = std::chrono::steady_clock::now();
            visualizer.swapBuffers();
            swapMs += msSince(swapStart);

            // Update stats once per second. glfwSetWindowTitle has to run on the
            // main thread, so just stash the numbers here and let main pick them up.
            frameCount++;
            auto now = std::chrono::steady_clock::now();
            std::chrono::duration<double> elapsed = now - statsTimerStart;
            if (elapsed.count() >= 1.0) {
                double fps = frameCount / elapsed.count();
                double avgFrameMs = (elapsed.count() * 1000.0) / frameCount;
                lastFps.store(fps, std::memory_order_relaxed);
                lastFrameTimeMs.store(avgFrameMs, std::memory_order_relaxed);
                titleNeedsUpdate.store(true, std::memory_order_relaxed);

                // fill runs on its own thread at its own rate, so average per call not per frame
                int fillCalls = fillCallCount.exchange(0, std::memory_order_relaxed);
                double fillMsAvg = fillCalls > 0
                    ? (fillTimeNs.exchange(0, std::memory_order_relaxed) / 1e6) / fillCalls
                    : 0.0;

                std::cout << "[Perf] FPS: " << fps
                           << " | Frame avg/worst: " << avgFrameMs << " / " << worstFrameMs << " ms"
                           << " | FFT: " << (fftMs / frameCount) << " ms"
                           << " | Draw: " << (drawMs / frameCount) << " ms"
                           << " | VSync wait: " << (swapMs / frameCount) << " ms"
                           << " | Fill: " << fillMsAvg << " ms (" << fillCalls << " calls)"
                           << " | Underruns: " << output.getUnderrunCount() << "\n";

                frameCount = 0;
                statsTimerStart = now;
                fftMs = 0.0;
                drawMs = 0.0;
                swapMs = 0.0;
                worstFrameMs = 0.0;
            }

            // Stop if buffer drained
            if (audioFileEnded.load(std::memory_order_acquire) && buffer.getAvailableReadSamples() == 0) {
                std::cout << "Playback finished.\n";
                output.stop();
                break;
            }
        }

        // Whole run summary
        if (!allFrameTimesMs.empty()) {
            std::sort(allFrameTimesMs.begin(), allFrameTimesMs.end());
            std::cout << "\n[Perf] Run summary (" << allFrameTimesMs.size() << " frames)\n"
                      << "  Frame time  p50: " << percentile(allFrameTimesMs, 50.0) << " ms"
                      << " | p99: " << percentile(allFrameTimesMs, 99.0) << " ms"
                      << " | p99.9: " << percentile(allFrameTimesMs, 99.9) << " ms"
                      << " | max: " << allFrameTimesMs.back() << " ms\n";
        }
        int underruns = audioFileEnded.load(std::memory_order_acquire)
            ? underrunsAtEof.load(std::memory_order_relaxed)
            : output.getUnderrunCount();
        std::cout << "  Underruns: " << underruns << " (ring buffer ran dry)"
                  << " | Device underflows: " << output.getDeviceUnderflowCount() << "\n";

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
