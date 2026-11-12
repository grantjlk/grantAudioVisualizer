#ifndef SPSC_RING_BUFFER_H
#define SPSC_RING_BUFFER_H

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <stdexcept>
#include <vector>

/**
* @class SpscRingBuffer
* @brief Lock-free single-producer/single-consumer ring buffer
*
* One thread writes and one other thread reads, no locks. The indices only count up and never wrap,
* so writeIndex - readIndex is always how many items are in the buffer, and the slot is index & mask
* (capacity has to be a power of two). Each thread caches the other thread's index so most calls
* don't touch the other core's cache line.
*
* @tparam T Element type. Meant for plain data like float, a type like std::string would still work but
* its copies can allocate, which you don't want on the audio thread
*/
template <typename T>
class SpscRingBuffer{
	private:
		static constexpr size_t cacheLineSize = 64;

		// these never change after construction, so both cores can keep this line cached
		std::vector<T> buffer;				///< Storage for the elements
		const size_t capacity;				///< Number of slots (power of two)
		const size_t mask;					///< capacity - 1, turns an index into a slot

		// each on its own cache line so the two threads don't invalidate each other's (false sharing)
		alignas(cacheLineSize) std::atomic<size_t> writeIndex;	///< Total elements written, only the producer stores it
		alignas(cacheLineSize) size_t cachedReadIndex;			///< Producer's copy of readIndex
		alignas(cacheLineSize) std::atomic<size_t> readIndex;	///< Total elements read, only the consumer stores it
		alignas(cacheLineSize) size_t cachedWriteIndex;			///< Consumer's copy of writeIndex

	public:
		/// @brief Construct a ring buffer with the given number of slots
		/// @param capacity Number of slots, has to be a power of two
		/// @throws std::invalid_argument if capacity isn't a power of two
		explicit SpscRingBuffer(size_t capacity);

		/// @brief Write up to count elements (producer thread only)
		/// @param data Elements to write
		/// @param count Number of elements to attempt writing
		/// @return Number of elements actually written
		size_t write(const T* data, size_t count);

		/// @brief Read up to count elements (consumer thread only)
		/// @param output Destination for the elements
		/// @param count Number of elements requested
		/// @return Number of elements actually read
		size_t read(T* output, size_t count);

		/// @brief Elements ready to read, only an estimate when called from another thread
		size_t readAvailable() const;

		/// @brief Free slots to write into, only an estimate when called from another thread
		size_t writeAvailable() const {return capacity - readAvailable();}

		/// @brief Total number of slots
		size_t getCapacity() const {return capacity;}

		// Disable copy constructor and assignment operator
		SpscRingBuffer(const SpscRingBuffer&) = delete;
		SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;
};

// template, so the definitions have to stay in the header

template <typename T>
SpscRingBuffer<T>::SpscRingBuffer(size_t capacity) : buffer(capacity), capacity(capacity), mask(capacity - 1), writeIndex(0), cachedReadIndex(0), readIndex(0), cachedWriteIndex(0){
	if(capacity == 0 || (capacity & (capacity - 1)) != 0){
		throw std::invalid_argument("SpscRingBuffer capacity has to be a power of two");
	}
}

template <typename T>
size_t SpscRingBuffer<T>::write(const T* data, size_t count){
	// Our own index can be relaxed, nobody else changes it
	size_t write = writeIndex.load(std::memory_order_relaxed);
	// Cached copy can only be behind the real one, so only reload it when it looks full
	if(capacity - (write - cachedReadIndex) < count){
		cachedReadIndex = readIndex.load(std::memory_order_acquire);
	}

	count = std::min(count, capacity - (write - cachedReadIndex));
	if(count == 0){
		return 0;
	}

	// Copy in two parts in case it wraps around the end
	size_t start = write & mask;
	size_t firstPart = std::min(count, capacity - start);
	std::copy(data, data + firstPart, buffer.data() + start);
	std::copy(data + firstPart, data + count, buffer.data());

	// Release so the consumer can't see the new index before the data
	writeIndex.store(write + count, std::memory_order_release);
	return count;
}

template <typename T>
size_t SpscRingBuffer<T>::read(T* output, size_t count){
	// Same as write but mirrored, only reload the cached copy when it looks empty
	size_t read = readIndex.load(std::memory_order_relaxed);
	if(cachedWriteIndex - read < count){
		cachedWriteIndex = writeIndex.load(std::memory_order_acquire);
	}

	count = std::min(count, cachedWriteIndex - read);
	if(count == 0){
		return 0;
	}

	size_t start = read & mask;
	size_t firstPart = std::min(count, capacity - start);
	std::copy(buffer.data() + start, buffer.data() + start + firstPart, output);
	std::copy(buffer.data(), buffer.data() + (count - firstPart), output + firstPart);

	// Release so the producer can't overwrite slots we're still copying out of
	readIndex.store(read + count, std::memory_order_release);
	return count;
}

template <typename T>
size_t SpscRingBuffer<T>::readAvailable() const{
	// Load readIndex first so write can't end up behind read when called from another thread
	size_t read = readIndex.load(std::memory_order_acquire);
	size_t write = writeIndex.load(std::memory_order_acquire);
	return std::min(write - read, capacity);
}

#endif
