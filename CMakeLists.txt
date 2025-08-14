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

add_executable(AudioVisualizer 
    src/main.cpp
    src/AudioLoader.cpp
)

target_include_directories(AudioVisualizer PRIVATE 
    include
)

# Link libraries
target_link_libraries(AudioVisualizer 
    glfw 
    OpenGL::GL
    portaudio
    SndFile::sndfile
)