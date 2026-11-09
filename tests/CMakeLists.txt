include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
# use the same runtime library as the rest of the project on msvc, otherwise it won't link
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
# don't install gtest alongside the project
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# registers each TEST() with ctest so `ctest` runs them individually
include(GoogleTest)
find_package(Threads REQUIRED)

# header only and no dependencies, so this one builds anywhere (eg. WSL for thread sanitizer)
add_executable(SpscRingBufferTests SpscRingBufferTests.cpp)
target_include_directories(SpscRingBufferTests PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(SpscRingBufferTests PRIVATE GTest::gtest_main Threads::Threads)
target_compile_options(SpscRingBufferTests PRIVATE ${AUDIOVIS_WARNINGS})
gtest_discover_tests(SpscRingBufferTests)

# these need libsndfile and fftw, so only when the app (and audio_core) is being built
if(TARGET audio_core)
    add_executable(AudioTests
        TestHelpers.cpp
        AudioBufferTests.cpp
        AudioAnalyzerTests.cpp
    )
    target_link_libraries(AudioTests PRIVATE audio_core GTest::gtest_main)
    target_compile_options(AudioTests PRIVATE ${AUDIOVIS_WARNINGS})
    gtest_discover_tests(AudioTests)
endif()
