cmake_minimum_required(VERSION 3.16)
project(grantAudioVisualizer
    VERSION 1.0
    DESCRIPTION "A simple audio visualizer using OpenGL and PortAudio"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# turn off to build only the parts that don't need the vcpkg libraries (eg. benchmark in WSL)
option(AUDIOVIS_BUILD_APP "Build the visualizer app (needs the vcpkg dependencies)" ON)

# portaudio's ring buffer as its own small library, so the app and the benchmark can both
# use it. it's just plain C with no other portaudio dependencies
add_library(pa_ringbuffer STATIC third_party/portaudio/pa_ringbuffer.c)
target_include_directories(pa_ringbuffer PUBLIC third_party/portaudio)

if(AUDIOVIS_BUILD_APP)
    find_package(glfw3 CONFIG REQUIRED)
    find_package(OpenGL REQUIRED)
    find_package(portaudio CONFIG REQUIRED)
    find_package(SndFile CONFIG REQUIRED)
    find_package(fftw3f CONFIG REQUIRED)
    find_package(glad CONFIG REQUIRED)
    find_package(tinyfiledialogs CONFIG REQUIRED)

    add_executable(AudioVisualizer
        src/main.cpp
        src/AudioLoader.cpp
        src/AudioBuffer.cpp
        src/AudioOutput.cpp
        src/AudioAnalyzer.cpp
        src/Visualizer.cpp
    )

    target_include_directories(AudioVisualizer PRIVATE include)

    # Link libraries
    target_link_libraries(AudioVisualizer PRIVATE
        pa_ringbuffer
        glfw
        OpenGL::GL
        portaudio
        SndFile::sndfile
        FFTW3::fftw3f
        glad::glad
        tinyfiledialogs::tinyfiledialogs
    )
endif()
