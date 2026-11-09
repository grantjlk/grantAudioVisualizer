cmake_minimum_required(VERSION 3.16)
project(grantAudioVisualizer
    VERSION 1.0
    DESCRIPTION "A simple audio visualizer using OpenGL and PortAudio"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# warnings for our own code only (not portaudio's ring buffer or googletest)
if(MSVC)
    # C4324 just says alignas added padding, which is the whole point of it in the ring buffer
    set(AUDIOVIS_WARNINGS /W4 /permissive- /wd4324)
else()
    set(AUDIOVIS_WARNINGS -Wall -Wextra)
endif()

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

    # loading, buffering and analysis, split out from the app so the tests can link it
    # without needing a window or an audio device
    add_library(audio_core STATIC
        src/AudioLoader.cpp
        src/AudioBuffer.cpp
        src/AudioAnalyzer.cpp
    )
    target_include_directories(audio_core PUBLIC include)
    target_compile_options(audio_core PRIVATE ${AUDIOVIS_WARNINGS})
    target_link_libraries(audio_core PUBLIC
        pa_ringbuffer
        SndFile::sndfile
        FFTW3::fftw3f
    )

    add_executable(AudioVisualizer
        src/main.cpp
        src/AudioOutput.cpp
        src/Visualizer.cpp
    )

    # Link libraries
    target_link_libraries(AudioVisualizer PRIVATE
        audio_core
        glfw
        OpenGL::GL
        portaudio
        glad::glad
        tinyfiledialogs::tinyfiledialogs
    )
    target_compile_options(AudioVisualizer PRIVATE ${AUDIOVIS_WARNINGS})
endif()

# ring buffer throughput benchmark, no vcpkg libraries needed either
option(AUDIOVIS_BUILD_BENCH "Build the ring buffer benchmark" ON)
if(AUDIOVIS_BUILD_BENCH)
    find_package(Threads REQUIRED)
    add_executable(RingBufferBench bench/RingBufferBench.cpp)
    target_include_directories(RingBufferBench PRIVATE include)
    target_link_libraries(RingBufferBench PRIVATE pa_ringbuffer Threads::Threads)
    target_compile_options(RingBufferBench PRIVATE ${AUDIOVIS_WARNINGS})
    # msvc flags sscanf as "unsafe" and wants its own sscanf_s, plain sscanf is fine for parsing --cpus
    if(MSVC)
        target_compile_definitions(RingBufferBench PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
endif()

# outside the app block since the ring buffer tests don't need any vcpkg libraries
option(AUDIOVIS_BUILD_TESTS "Build the unit tests (downloads GoogleTest)" ON)
if(AUDIOVIS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
