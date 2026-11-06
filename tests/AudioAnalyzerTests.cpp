#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "AudioAnalyzer.h"
#include "TestHelpers.h"

constexpr int kSampleRate = 44100;
constexpr int kFftSize = 1024;
constexpr size_t kTapSize = 8192;

// frequency that lands exactly on fft bin `bin` (each bin is sampleRate / fftSize = ~43 Hz wide).
// exactly on a bin means a whole number of cycles fits in the window, so there's no leakage guesswork
static float binFrequency(int bin){
	return static_cast<float>(bin) * kSampleRate / kFftSize;
}

// pretend to be AudioOutput's callback, pushing "played" mono audio into the tap
static void play(SpscRingBuffer<float>& tap, const std::vector<float>& samples){
	ASSERT_EQ(tap.write(samples.data(), samples.size()), samples.size());
}

static int loudestBucket(const AudioAnalyzer& analyzer){
	const std::vector<float>& buckets = analyzer.getBuckets();
	return static_cast<int>(std::max_element(buckets.begin(), buckets.end()) - buckets.begin());
}

TEST(AudioAnalyzerTest, WaitsForAFullWindow){
	SpscRingBuffer<float> tap(kTapSize);
	AudioAnalyzer analyzer(&tap, kFftSize, kSampleRate);

	play(tap, makeSine(500, 440.0f, 0.5f));
	EXPECT_FALSE(analyzer.analyzeNextBlock());
	// the first 500 aren't thrown away, 524 more completes the window
	play(tap, makeSine(524, 440.0f, 0.5f));
	EXPECT_TRUE(analyzer.analyzeNextBlock());
}

TEST(AudioAnalyzerTest, OnlyAnalyzesWhenNewAudioWasPlayed){
	SpscRingBuffer<float> tap(kTapSize);
	AudioAnalyzer analyzer(&tap, kFftSize, kSampleRate);

	play(tap, makeSine(kFftSize, 440.0f, 0.5f));
	EXPECT_TRUE(analyzer.analyzeNextBlock());
	// nothing played since, so nothing new to show
	EXPECT_FALSE(analyzer.analyzeNextBlock());
}

TEST(AudioAnalyzerTest, SilenceGivesZeroBuckets){
	SpscRingBuffer<float> tap(kTapSize);
	AudioAnalyzer analyzer(&tap, kFftSize, kSampleRate);

	play(tap, std::vector<float>(kFftSize, 0.0f));
	ASSERT_TRUE(analyzer.analyzeNextBlock());
	for(float bucket : analyzer.getBuckets()){
		EXPECT_FLOAT_EQ(bucket, 0.0f);
	}
	EXPECT_FLOAT_EQ(analyzer.getRmsVal(), 0.0f);
	EXPECT_FLOAT_EQ(analyzer.getPeakAmplitude(), 0.0f);
}

TEST(AudioAnalyzerTest, UsesOnlyTheLatestSamplesWhenALotArrivesAtOnce){
	SpscRingBuffer<float> tap(kTapSize);
	AudioAnalyzer analyzer(&tap, kFftSize, kSampleRate);

	// eg. the render thread stalled for a bit, so two windows worth arrive in one frame.
	// only the newer one (bin 100 -> bucket 25) should show
	play(tap, makeSine(kFftSize, binFrequency(27), 0.5f));
	play(tap, makeSine(kFftSize, binFrequency(100), 0.5f));
	ASSERT_TRUE(analyzer.analyzeNextBlock());
	EXPECT_EQ(loudestBucket(analyzer), 25);
}

TEST(AudioAnalyzerTest, WindowSlidesForward){
	SpscRingBuffer<float> tap(kTapSize);
	AudioAnalyzer analyzer(&tap, kFftSize, kSampleRate);

	play(tap, std::vector<float>(kFftSize, 0.0f));
	ASSERT_TRUE(analyzer.analyzeNextBlock());

	// half a window of sine, so the window is now 512 zeros then 512 sine samples.
	// rms over the whole window = sqrt(half * amplitude^2 / 2) = 0.5 * 0.5 = 0.25
	play(tap, makeSine(kFftSize / 2, binFrequency(27), 0.5f));
	ASSERT_TRUE(analyzer.analyzeNextBlock());
	EXPECT_NEAR(analyzer.getRmsVal(), 0.25f, 1e-3f);
	EXPECT_NEAR(analyzer.getPeakAmplitude(), 0.5f, 1e-2f);
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

	SpscRingBuffer<float> tap(kTapSize);
	AudioAnalyzer analyzer(&tap, kFftSize, kSampleRate);
	play(tap, makeSine(kFftSize, binFrequency(c.bin), amplitude, kSampleRate));
	ASSERT_TRUE(analyzer.analyzeNextBlock());

	EXPECT_EQ(loudestBucket(analyzer), c.expectedBucket);
	// rms of a sine is amplitude / sqrt(2), and the peak should be (almost) the amplitude
	EXPECT_NEAR(analyzer.getRmsVal(), amplitude / std::sqrt(2.0f), 1e-3f);
	EXPECT_NEAR(analyzer.getPeakAmplitude(), amplitude, 1e-2f);
}

INSTANTIATE_TEST_SUITE_P(KnownFrequencies, AudioAnalyzerSineTest,
	::testing::Values(SineCase{27, 19}, SineCase{100, 25}),
	// names the tests eg. "Bin27" instead of a hex dump of the struct
	[](const ::testing::TestParamInfo<SineCase>& info){ return "Bin" + std::to_string(info.param.bin); });
