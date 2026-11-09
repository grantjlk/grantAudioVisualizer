#include <algorithm>
#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <thread>
#include <vector>
#include <gtest/gtest.h>
#include "SpscRingBuffer.h"

TEST(SpscRingBufferTest, StartsEmpty){
	SpscRingBuffer<float> buffer(8);
	EXPECT_EQ(buffer.readAvailable(), 0u);
	EXPECT_EQ(buffer.writeAvailable(), 8u);

	float out[4];
	EXPECT_EQ(buffer.read(out, 4), 0u);
}

TEST(SpscRingBufferTest, RejectsNonPowerOfTwoCapacity){
	EXPECT_THROW(SpscRingBuffer<float>(0), std::invalid_argument);
	EXPECT_THROW(SpscRingBuffer<float>(6), std::invalid_argument);
	EXPECT_NO_THROW(SpscRingBuffer<float>(1));
	EXPECT_NO_THROW(SpscRingBuffer<float>(1024));
}

TEST(SpscRingBufferTest, ReadsBackWhatWasWrittenInOrder){
	SpscRingBuffer<int> buffer(8);
	int in[5] = {1, 2, 3, 4, 5};
	EXPECT_EQ(buffer.write(in, 5), 5u);
	EXPECT_EQ(buffer.readAvailable(), 5u);

	int out[5] = {};
	EXPECT_EQ(buffer.read(out, 5), 5u);
	for(int i = 0; i < 5; i++){
		EXPECT_EQ(out[i], in[i]);
	}
	EXPECT_EQ(buffer.readAvailable(), 0u);
}

TEST(SpscRingBufferTest, UsesEverySlotAndStopsWhenFull){
	SpscRingBuffer<int> buffer(8);
	std::vector<int> in(12, 7);
	// all 8 slots usable since the indices never wrap (no wasted slot)
	EXPECT_EQ(buffer.write(in.data(), 12), 8u);
	EXPECT_EQ(buffer.writeAvailable(), 0u);
	EXPECT_EQ(buffer.write(in.data(), 1), 0u);
}

TEST(SpscRingBufferTest, PartialReadLeavesTheRest){
	SpscRingBuffer<int> buffer(8);
	int in[6] = {0, 1, 2, 3, 4, 5};
	buffer.write(in, 6);

	int out[4] = {};
	EXPECT_EQ(buffer.read(out, 4), 4u);
	EXPECT_EQ(buffer.readAvailable(), 2u);
	EXPECT_EQ(buffer.read(out, 4), 2u);
	EXPECT_EQ(out[0], 4);
	EXPECT_EQ(out[1], 5);
}

TEST(SpscRingBufferTest, StaysInOrderAcrossWraparound){
	SpscRingBuffer<int> buffer(16);
	// odd sizes so writes and reads split across the end of the storage at different spots
	int next = 0;
	int expected = 0;
	std::vector<int> chunk(11);
	std::vector<int> out(7);
	for(int round = 0; round < 1000; round++){
		for(int& v : chunk){
			v = next++;
		}
		size_t written = buffer.write(chunk.data(), chunk.size());
		next -= static_cast<int>(chunk.size() - written);	// whatever didn't fit gets written next round

		size_t got = buffer.read(out.data(), out.size());
		for(size_t i = 0; i < got; i++){
			ASSERT_EQ(out[i], expected++);
		}
	}
}

TEST(SpscRingBufferTest, IndexSubtractionWorksAcrossOverflow){
	// can't write 2^64 elements in a test, so this checks the same math on a smaller scale:
	// unsigned subtraction still gives the right count after the counter wraps around
	size_t write = 2;
	size_t read = SIZE_MAX - 1;
	EXPECT_EQ(write - read, 4u);
}

// the real point of the buffer: one thread writing while another reads. every value has to arrive
// exactly once and in order, so a lost, duplicated or out of order element fails the test
TEST(SpscRingBufferTest, TwoThreadStress){
	SpscRingBuffer<uint32_t> buffer(256);
	const uint32_t total = 2'000'000;
	// if the consumer bails early the producer would spin forever on a full buffer, so tell it to stop
	std::atomic<bool> consumerFailed{false};

	std::thread producer([&]() {
		std::vector<uint32_t> chunk(64);
		uint32_t next = 0;
		while(next < total && !consumerFailed.load()){
			// changing chunk sizes (1-37) so the indices land everywhere
			size_t count = std::min<size_t>(next % 37 + 1, total - next);
			for(size_t i = 0; i < count; i++){
				chunk[i] = next + static_cast<uint32_t>(i);
			}
			size_t written = buffer.write(chunk.data(), count);
			next += static_cast<uint32_t>(written);
			if(written == 0){
				std::this_thread::yield();
			}
		}
	});

	std::vector<uint32_t> out(64);
	uint32_t expected = 0;
	bool inOrder = true;
	while(expected < total && inOrder){
		size_t got = buffer.read(out.data(), (expected % 53) + 1);
		for(size_t i = 0; i < got; i++){
			if(out[i] != expected){
				inOrder = false;
				consumerFailed.store(true);
				break;
			}
			expected++;
		}
		if(got == 0){
			std::this_thread::yield();
		}
	}
	producer.join();

	EXPECT_TRUE(inOrder) << "first wrong value was at position " << expected;
	EXPECT_EQ(expected, total);
}
