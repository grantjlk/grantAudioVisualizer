#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <vector>
#include "AudioLoader.h"

/// @brief Writes samples to a temporary float WAV file and loads it with the given AudioLoader, the same way the app loads a real file
/// @param loader AudioLoader to load into (has to outlive any AudioBuffer made from it)
/// @param samples Interleaved samples to write
/// @param channels Number of channels in samples
/// @param sampleRate Sample rate (Hz)
/// @return True if the file was written and loaded
bool loadSamples(AudioLoader& loader, const std::vector<float>& samples, int channels = 1, int sampleRate = 44100);

/// @brief Makes a ramp 0, 1, 2, ... so every sample is unique and order is easy to check
/// @param count Number of samples
std::vector<float> makeRamp(int count);

/// @brief Makes a mono sine wave
/// @param count Number of samples
/// @param freq Frequency (Hz)
/// @param amplitude Peak amplitude
/// @param sampleRate Sample rate (Hz)
std::vector<float> makeSine(int count, float freq, float amplitude, int sampleRate = 44100);

#endif
