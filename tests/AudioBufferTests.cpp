#include <vector>
#include <gtest/gtest.h>
#include "AudioBuffer.h"
#include "TestHelpers.h"

// small power of two, portaudio's ring buffer only accepts power of two sizes
constexpr int kBufferSize = 16;

TEST(AudioBufferTest, ReadsBackWhatWasFilledInOrder){
	AudioLoader loader;
	std::vector<float> source = makeRamp(10);
	ASSERT_TRUE(loadSamples(loader, source));
	AudioBuffer buffer(kBufferSize, loader);

	buffer.fillBuffer(10);
	std::vector<float> out(10);
	EXPECT_EQ(buffer.readBuffer(out.data(), 10), 10);
	EXPECT_EQ(out, source);
}

TEST(AudioBufferTest, FillStopsWhenBufferIsFull){
	AudioLoader loader;
	ASSERT_TRUE(loadSamples(loader, makeRamp(100)));
	AudioBuffer buffer(kBufferSize, loader);

	// asks for way more than fits, should only write what fits and say there's more left
	EXPECT_TRUE(buffer.fillBuffer(100));
	EXPECT_EQ(buffer.getAvailableReadSamples(), kBufferSize);
}

TEST(AudioBufferTest, FillReturnsFalseAtEndOfSource){
	AudioLoader loader;
	ASSERT_TRUE(loadSamples(loader, makeRamp(4)));
	AudioBuffer buffer(kBufferSize, loader);

	EXPECT_FALSE(buffer.fillBuffer(8));
	// filling again after the end shouldn't add anything
	EXPECT_FALSE(buffer.fillBuffer(8));
	EXPECT_EQ(buffer.getAvailableReadSamples(), 4);
}

TEST(AudioBufferTest, ReadReturnsShortWhenNotEnoughData){
	AudioLoader loader;
	ASSERT_TRUE(loadSamples(loader, makeRamp(4)));
	AudioBuffer buffer(kBufferSize, loader);

	buffer.fillBuffer(4);
	// this is the underrun case, AudioOutput fills the rest with silence
	std::vector<float> out(8);
	EXPECT_EQ(buffer.readBuffer(out.data(), 8), 4);
	EXPECT_FALSE(buffer.hasData());
}

TEST(AudioBufferTest, StaysInOrderAcrossWraparound){
	AudioLoader loader;
	std::vector<float> source = makeRamp(1000);
	ASSERT_TRUE(loadSamples(loader, source));
	AudioBuffer buffer(kBufferSize, loader);

	// odd fill/read sizes so the indices wrap at a different spot each time, and reads
	// split across the end of the buffer
	std::vector<float> collected;
	std::vector<float> chunk(7);
	bool more = true;
	while(more || buffer.hasData()){
		more = buffer.fillBuffer(11);
		int got = buffer.readBuffer(chunk.data(), 7);
		collected.insert(collected.end(), chunk.begin(), chunk.begin() + got);
	}
	EXPECT_EQ(collected, source);
}
