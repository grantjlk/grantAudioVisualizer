#ifndef BUFFER_ADAPTERS_H
#define BUFFER_ADAPTERS_H

#include <cstddef>
#include <cstdint>
#include <vector>
#include "pa_ringbuffer.h"
#include "SpscRingBuffer.h"

// Every buffer in the benchmark gets wrapped in the same tiny interface, so the harness code is
// identical for all of them and the only thing that changes between runs is the buffer itself:
//   static constexpr const char* name
//   explicit Adapter(size_t capacity)
//   size_t write(const Sample* data, size_t count)   // returns how many were written
//   size_t read(Sample* out, size_t count)           // returns how many were read

// 4 bytes like the app's float samples, but an integer counter stays exact all the way up
// (a float can't count past 16.7M without skipping), so the consumer can check every value
using Sample = uint32_t;

/// @brief PortAudio's PaUtilRingBuffer, what the app uses between the fill thread and the audio callback
struct PaUtilAdapter{
	static constexpr const char* name = "paUtil";

	std::vector<Sample> storage;
	PaUtilRingBuffer ringBuffer;

	explicit PaUtilAdapter(size_t capacity) : storage(capacity){
		PaUtil_InitializeRingBuffer(&ringBuffer, sizeof(Sample), static_cast<ring_buffer_size_t>(capacity), storage.data());
	}

	size_t write(const Sample* data, size_t count){
		return static_cast<size_t>(PaUtil_WriteRingBuffer(&ringBuffer, data, static_cast<ring_buffer_size_t>(count)));
	}

	size_t read(Sample* out, size_t count){
		return static_cast<size_t>(PaUtil_ReadRingBuffer(&ringBuffer, out, static_cast<ring_buffer_size_t>(count)));
	}
};

/// @brief Our own SpscRingBuffer
struct SpscAdapter{
	static constexpr const char* name = "spsc";

	SpscRingBuffer<Sample> ringBuffer;

	explicit SpscAdapter(size_t capacity) : ringBuffer(capacity) {}

	size_t write(const Sample* data, size_t count){ return ringBuffer.write(data, count); }
	size_t read(Sample* out, size_t count){ return ringBuffer.read(out, count); }
};

#endif
