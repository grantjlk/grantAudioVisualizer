cmake_minimum_required(VERSION 3.16)
project(grantAudioVisualizer
    VERSION 1.0
    DESCRIPTION "A simple audio visualizer using OpenGL and PortAudio"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(portaudio CONFIG REQUIRED)
find_package(SndFile CONFIG REQUIRED)
find_package(fftw3 CONFIG REQUIRED)

set_source_files_properties(
    third_party/portaudio/pa_ringbuffer.c
    PROPERTIES LANGUAGE C
)

add_executable(AudioVisualizer 
    src/main.cpp
    src/AudioLoader.cpp
    src/AudioBuffer.cpp
    src/AudioOutput.cpp
    third_party/portaudio/pa_ringbuffer.c
)

target_include_directories(AudioVisualizer PRIVATE 
    include
    third_party/portaudio
)

# Link libraries
target_link_libraries(AudioVisualizer 
    glfw 
    OpenGL::GL
    portaudio
    SndFile::sndfile
    FFTW3::fftw3f
)