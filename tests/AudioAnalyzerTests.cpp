#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "AudioAnalyzer.h"
#include "TestHelpers.h"

constexpr int kSampleRate = 44100;
constexpr int kFftSize = 1024;
constexpr int kBufferSize = 2048;

// frequency that lands exactly on fft bin `bin` (each bin is sampleRate / fftSize = ~43 Hz wide).
// exactly on a bin means a whole number of cycles fits in the window, so there's no leakage guesswork
static float binFrequency(int bin){
	return static_cast<float>(bin) * kSampleRate / kFftSize;
}

TEST(AudioAnalyzerTest, NeedsAFullBlockToAnalyze){
	AudioLoader loader;
	ASSERT_TRUE(loadSamples(loader, makeSine(500, 440.0f, 0.5f), 1, kSampleRate));
	AudioBuffer buffer(kBufferSize, loader);
	buffer.fillBuffer(500);

	AudioAnalyzer analyzer(&buffer, kFftSize, kSampleRate);
	EXPECT_FALSE(analyzer.analyzeNextBlock());
}

TEST(AudioAnalyzerTest, SilenceGivesZeroBuckets){
	AudioLoader loader;
	ASSERT_TRUE(loadSamples(loader, std::vector<float>(kFftSize, 0.0f), 1, kSampleRate));
	AudioBuffer buffer(kBufferSize, loader);
	buffer.fillBuffer(kFftSize);

	AudioAnalyzer analyzer(&buffer, kFftSize, kSampleRate);
	ASSERT_TRUE(analyzer.analyzeNextBlock());
	for(float bucket : analyzer.getBuckets()){
		EXPECT_FLOAT_EQ(bucket, 0.0f);
	}
	EXPECT_FLOAT_EQ(analyzer.getRmsVal(), 0.0f);
	EXPECT_FLOAT_EQ(analyzer.getPeakAmplitude(), 0.0f);
}

// bin 27 (~1163 Hz) is only in bucket 19 (bins 24-30), and bin 100 (~4307 Hz) is only in
// bucket 25 (bins 86-106). picked away from bucket edges since neighbouring buckets share edge bins
struct SineCase{
	int bin;
	int expectedBucket;
};

// so failures/ctest print "bin 27 -> bucket 19" instead of raw bytes
static void PrintTo(const SineCase& c, std::ostream* os){
	*os << "bin " << c.bin << " -> bucket " << c.expectedBucket;
}

class AudioAnalyzerSineTest : public ::testing::TestWithParam<SineCase> {};

TEST_P(AudioAnalyzerSineTest, SineLandsInExpectedBucket){
	const SineCase& c = GetParam();
	const float amplitude = 0.5f;

	AudioLoader loader;
	ASSERT_TRUE(loadSamples(loader, makeSine(kFftSize, binFrequency(c.bin), amplitude, kSampleRate), 1, kSampleRate));
	AudioBuffer buffer(kBufferSize, loader);
	buffer.fillBuffer(kFftSize);

	AudioAnalyzer analyzer(&buffer, kFftSize, kSampleRate);
	ASSERT_TRUE(analyzer.analyzeNextBlock());

	const std::vector<float>& buckets = analyzer.getBuckets();
	int loudest = static_cast<int>(std::max_element(buckets.begin(), buckets.end()) - buckets.begin());
	EXPECT_EQ(loudest, c.expectedBucket);

	// rms of a sine is amplitude / sqrt(2), and the peak should be (almost) the amplitude
	EXPECT_NEAR(analyzer.getRmsVal(), amplitude / std::sqrt(2.0f), 1e-3f);
	EXPECT_NEAR(analyzer.getPeakAmplitude(), amplitude, 1e-2f);
}

INSTANTIATE_TEST_SUITE_P(KnownFrequencies, AudioAnalyzerSineTest,
	::testing::Values(SineCase{27, 19}, SineCase{100, 25}),
	// names the tests eg. "Bin27" instead of a hex dump of the struct
	[](const ::testing::TestParamInfo<SineCase>& info){ return "Bin" + std::to_string(info.param.bin); });
