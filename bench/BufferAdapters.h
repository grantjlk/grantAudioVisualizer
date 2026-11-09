#ifndef BUFFER_ADAPTERS_H
#define BUFFER_ADAPTERS_H

#include <cstddef>
#include <cstdint>
#include <vector>
#include "pa_ringbuffer.h"
#include "rigtorp/SPSCQueue.h"
#include "SpscRingBuffer.h"

// Every buffer in the benchmark gets wrapped in the same tiny interface, so the harness code is
// identical for all of them and the only thing that changes between runs is the buffer itself:
//   static constexpr const char* name
//   static constexpr bool bulk                       // false = one element per call, only run at chunk 1
//   explicit Adapter(size_t capacity)
//   size_t write(const Sample* data, size_t count)   // returns how many were written
//   size_t read(Sample* out, size_t count)           // returns how many were read

// 4 bytes like the app's float samples, but an integer counter stays exact all the way up
// (a float can't count past 16.7M without skipping), so the consumer can check every value
using Sample = uint32_t;

/// @brief PortAudio's PaUtilRingBuffer, what the app uses between the fill thread and the audio callback
struct PaUtilAdapter{
	static constexpr const char* name = "paUtil";
	static constexpr bool bulk = true;

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
	static constexpr bool bulk = true;

	SpscRingBuffer<Sample> ringBuffer;

	explicit SpscAdapter(size_t capacity) : ringBuffer(capacity) {}

	size_t write(const Sample* data, size_t count){ return ringBuffer.write(data, count); }
	size_t read(Sample* out, size_t count){ return ringBuffer.read(out, count); }
};

/// @brief Erik Rigtorp's SPSCQueue (third_party/rigtorp, MIT), a well known reference for this kind of queue.
/// Same cached index + padding design as our v3, but it pushes/pops one element per call, so it's only
/// compared at chunk 1 (looping it 512 times wouldn't be a fair bulk comparison)
struct RigtorpAdapter{
	static constexpr const char* name = "rigtorp";
	static constexpr bool bulk = false;

	// it keeps one slot empty internally (allocates capacity + 1), so this is the same usable capacity
	rigtorp::SPSCQueue<Sample> queue;

	explicit RigtorpAdapter(size_t capacity) : queue(capacity) {}

	size_t write(const Sample* data, size_t count){
		size_t written = 0;
		while(written < count && queue.try_push(data[written])){
			written++;
		}
		return written;
	}

	size_t read(Sample* out, size_t count){
		size_t got = 0;
		while(got < count){
			Sample* front = queue.front();
			if(!front){
				break;
			}
			out[got++] = *front;
			queue.pop();
		}
		return got;
	}
};

#endif
