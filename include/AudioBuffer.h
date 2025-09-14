#ifndef AUDIO_BUFFER_H
#define AUDIO_BUFFER_H	

#include <vector>
#include <cstddef>
#include <algorithm>
#include "AudioLoader.h"
#include "pa_ringbuffer.h"

/**
* @class AudioBuffer 
* @brief Thread-safe audio data buffer using PortAudio's ring buffer
*
* Wraps PortAudio's PaUtilRingBuffer to provide thread-safe transfer of audio data between threads.
*/
class AudioBuffer{
	private:
		const std::vector<float>* audioData;	// Pointer to audio data loaded from AudioLoader
		size_t sourcePosition;					// Current read position in audio data
		float* bufferData;						// Memory used for ring buffer storage
		PaUtilRingBuffer ringBuffer;			
		
	public:
		/// @brief Construct an AudioBuffer with given size and loader for the audio source
		/// @param bufferSizeInSamples Size of buffer in samples
		/// @param loader AudioLoader containing data to fill the buffer
		AudioBuffer(int bufferSizeInSamples, const AudioLoader& loader);

		/// @brief Destructor deallocates buffer memory
		~AudioBuffer();


		/// @brief Fill the buffer with audio samples from the source data, typically called by main thread to keep buffer filled during audio playback
		/// @param samplesToWrite Number of samples to attempt writing into the ring buffer
		/// @return False if the end of the source file has been reached, true otherwise
		bool fillBuffer(int samplesToWrite);

		/// @brief Read samples from the ring buffer (destructive read), used directly inside of AudioOutput for audio stream
		/// @param output Destination array to store samples
		/// @param frameCount Number of samples requested
		/// @return Number of samples actually read
		int readBuffer(float* output, int frameCount);

		/// @brief Peek at samples from the ring buffer, without removing, used in AudioAnalyzer to analyze at the same time AudioOutput reads
		/// @param output Destination array to copy samples
		/// @param frameCount Number of samples to peek
		/// @return Number of samples successfully copied
		int peekBuffer(float* output, int frameCount);

		/// @brief Gets samples available to read in buffer, useful to check buffer status
		/// @return Number of readable samples
		int getAvailableReadSamples() const;

		//maybe remove? I don't utilize this rn
		bool hasData() const;


		// Disable copy constructor and assignment operator(buffer memory is unique per instance)
    	AudioBuffer(const AudioBuffer&) = delete;
    	AudioBuffer& operator=(const AudioBuffer&) = delete;
};
#endif