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

add_executable(AudioTests
    TestHelpers.cpp
    AudioBufferTests.cpp
    AudioAnalyzerTests.cpp
)
target_link_libraries(AudioTests PRIVATE audio_core GTest::gtest_main)

# registers each TEST() with ctest so `ctest` runs them individually
include(GoogleTest)
gtest_discover_tests(AudioTests)
