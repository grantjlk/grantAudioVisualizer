#define _USE_MATH_DEFINES
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <sndfile.h>
#include <gtest/gtest.h>
#include "TestHelpers.h"

bool loadSamples(AudioLoader& loader, const std::vector<float>& samples, int channels, int sampleRate){
	// name the file after the running test so tests never share a file
	const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
	std::string name = std::string(info->test_suite_name()) + "_" + info->name() + ".wav";
	// parameterized tests have slashes in their names, which would turn into folders
	std::replace(name.begin(), name.end(), '/', '_');
	std::filesystem::path path = std::filesystem::temp_directory_path() / name;

	SF_INFO sfInfo = {};
	sfInfo.samplerate = sampleRate;
	sfInfo.channels = channels;
	// float wav so the samples come back exactly as written (16 bit would round them)
	sfInfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;

	SNDFILE* file = sf_open(path.string().c_str(), SFM_WRITE, &sfInfo);
	if(!file){
		return false;
	}
	sf_writef_float(file, samples.data(), samples.size() / channels);
	sf_close(file);

	bool loaded = loader.loadAudioFile(path.string().c_str());
	std::filesystem::remove(path);
	return loaded;
}

std::vector<float> makeRamp(int count){
	std::vector<float> samples(count);
	for(int i = 0; i < count; i++){
		samples[i] = static_cast<float>(i);
	}
	return samples;
}

std::vector<float> makeSine(int count, float freq, float amplitude, int sampleRate){
	std::vector<float> samples(count);
	for(int i = 0; i < count; i++){
		samples[i] = amplitude * sinf(2.0f * static_cast<float>(M_PI) * freq * i / sampleRate);
	}
	return samples;
}
