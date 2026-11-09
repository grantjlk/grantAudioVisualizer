#ifndef AUDIO_BUFFER_H
#define AUDIO_BUFFER_H

#include <vector>
#include <cstddef>
#include <algorithm>
#include "AudioLoader.h"
#include "SpscRingBuffer.h"

/**
* @class AudioBuffer
* @brief Thread-safe audio data buffer between the fill thread and the audio callback
*
* Feeds samples from the loaded file into a lock-free SpscRingBuffer. The fill thread is the only writer
* (fillBuffer) and AudioOutput's callback is the only reader (readBuffer).
*/
class AudioBuffer{
	private:
		const std::vector<float>* audioData;	///< Pointer to audio data loaded from AudioLoader
		size_t sourcePosition;					///< Current read position in audio data
		SpscRingBuffer<float> ringBuffer;		///< Lock-free ring buffer the samples go through

	public:
		/// @brief Construct an AudioBuffer with given size and loader for the audio source
		/// @param bufferSizeInSamples Size of buffer in samples, has to be a power of two
		/// @param loader AudioLoader containing data to fill the buffer
		/// @throws std::invalid_argument if bufferSizeInSamples isn't a power of two
		AudioBuffer(int bufferSizeInSamples, const AudioLoader& loader);


		/// @brief Fill the buffer with audio samples from the source data, typically called by main thread to keep buffer filled during audio playback
		/// @param samplesToWrite Number of samples to attempt writing into the ring buffer
		/// @return False if the end of the source file has been reached, true otherwise
		bool fillBuffer(int samplesToWrite);

		/// @brief Read samples from the ring buffer (destructive read), used directly inside of AudioOutput for audio stream
		/// @param output Destination array to store samples
		/// @param frameCount Number of samples requested
		/// @return Number of samples actually read
		int readBuffer(float* output, int frameCount);

		/// @brief Gets samples available to read in buffer, useful to check buffer status
		/// @return Number of readable samples (an estimate when called from a thread other than the reader)
		int getAvailableReadSamples() const;

		//maybe remove? I don't utilize this rn
		bool hasData() const;


		// Disable copy constructor and assignment operator(buffer memory is unique per instance)
    	AudioBuffer(const AudioBuffer&) = delete;
    	AudioBuffer& operator=(const AudioBuffer&) = delete;
};
#endif
