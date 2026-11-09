#ifndef SPSC_RING_BUFFER_H
#define SPSC_RING_BUFFER_H

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <vector>

/**
* @class SpscRingBuffer
* @brief Lock-free single-producer/single-consumer ring buffer
*
* One thread writes, one other thread reads, no locks. Only two indices are shared between the threads:
* writeIndex (only the producer changes it) and readIndex (only the consumer changes it).
*
* The indices only ever count up and are never wrapped themselves. The slot is index & mask,
* which only works when the capacity is a power of two. Since they never wrap,
* (writeIndex - readIndex) is always the number of items in the buffer, even after the size_t overflows,
* so every slot can be used (no "one empty slot" trick to tell full from empty).
*
* @tparam T Element type, has to be trivially copyable since elements are copied around in bulk
*/
template <typename T>
class SpscRingBuffer{
	static_assert(std::is_trivially_copyable_v<T>, "SpscRingBuffer elements are copied in bulk, so T has to be trivially copyable");

	private:
		std::vector<T> buffer;				///< Storage for the elements
		const size_t capacity;				///< Number of slots (power of two)
		const size_t mask;					///< capacity - 1, turns an index into a slot
		std::atomic<size_t> writeIndex;		///< Total elements ever written, only the producer stores to it
		std::atomic<size_t> readIndex;		///< Total elements ever read, only the consumer stores to it

	public:
		/// @brief Construct a ring buffer with the given number of slots
		/// @param capacity Number of slots, has to be a power of two
		/// @throws std::invalid_argument if capacity isn't a power of two
		explicit SpscRingBuffer(size_t capacity)
			: buffer(capacity), capacity(capacity), mask(capacity - 1), writeIndex(0), readIndex(0)
		{
			// portaudio's buffer just returns -1 here and AudioBuffer ignored it, so fail loudly instead
			if(capacity == 0 || (capacity & (capacity - 1)) != 0){
				throw std::invalid_argument("SpscRingBuffer capacity has to be a power of two");
			}
		}

		/// @brief Write up to count elements (producer thread only)
		/// @param data Elements to write
		/// @param count Number of elements to attempt writing
		/// @return Number of elements actually written (less than count if the buffer filled up)
		size_t write(const T* data, size_t count){
			// relaxed: only this thread ever stores writeIndex, so it already knows the latest value
			size_t write = writeIndex.load(std::memory_order_relaxed);
			// acquire: pairs with the consumer's release store of readIndex, so once we see a slot as
			// free, the consumer is guaranteed to be done copying out of it
			size_t read = readIndex.load(std::memory_order_acquire);

			count = std::min(count, capacity - (write - read));

			// copy in up to two parts in case it wraps past the end of the storage
			size_t start = write & mask;
			size_t firstPart = std::min(count, capacity - start);
			std::copy(data, data + firstPart, buffer.data() + start);
			std::copy(data + firstPart, data + count, buffer.data());

			// release: everything written before this (the copy) is visible to whoever acquires the new
			// index, so the consumer never sees the new index before the data is there. on x86 this is a
			// plain mov, where v1's default seq_cst store was an xchg (a full fence) on every call
			writeIndex.store(write + count, std::memory_order_release);
			return count;
		}

		/// @brief Read up to count elements (consumer thread only)
		/// @param output Destination for the elements
		/// @param count Number of elements requested
		/// @return Number of elements actually read (less than count if the buffer ran out)
		size_t read(T* output, size_t count){
			// same idea mirrored: our own index is relaxed, the producer's is acquire (pairs with its
			// release), so the data it copied in is guaranteed visible before we read it
			size_t read = readIndex.load(std::memory_order_relaxed);
			size_t write = writeIndex.load(std::memory_order_acquire);

			count = std::min(count, write - read);

			size_t start = read & mask;
			size_t firstPart = std::min(count, capacity - start);
			std::copy(buffer.data() + start, buffer.data() + start + firstPart, output);
			std::copy(buffer.data(), buffer.data() + (count - firstPart), output + firstPart);

			// release: the copy out finishes before the slots show up as free, so the producer can't
			// overwrite them mid copy
			readIndex.store(read + count, std::memory_order_release);
			return count;
		}

		/// @brief Elements ready to read. Exact on the consumer thread, a lower bound on others
		size_t readAvailable() const{
			return writeIndex.load(std::memory_order_acquire) - readIndex.load(std::memory_order_acquire);
		}

		/// @brief Free slots to write into. Exact on the producer thread, a lower bound on others
		size_t writeAvailable() const{
			return capacity - (writeIndex.load(std::memory_order_acquire) - readIndex.load(std::memory_order_acquire));
		}

		/// @brief Total number of slots
		size_t getCapacity() const { return capacity; }

		// Disable copy constructor and assignment operator (atomics can't be copied, and two
		// copies sharing a producer/consumer would break the single-producer/single-consumer rule)
		SpscRingBuffer(const SpscRingBuffer&) = delete;
		SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;
};

#endif
