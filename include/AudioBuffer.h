#ifndef AUDIO_BUFFER_H
#define AUDIO_BUFFER_H	

#include <vector>
#include <cstddef>
#include <algorithm>
#include "AudioLoader.h"
#include "pa_ringbuffer.h"

/**
* AudioBuffer - Thread-safe audio data buffer using PortAudio's ring buffer
* 
* This class wraps PortAudio's PaUtilRingBuffer to provide a thread-safe way to
* transfer audio data between threads. It reads chunks from loaded audio file data
* and makes it available through a ring buffer interface.
* 
* Typical usage:
* - Main thread calls fillBuffer() to load data from AudioLoader into ring buffer
* - PortAudio callback thread calls readBuffer() to get data for audio output
* - Audio analyzer can also call readBuffer() to get data for FFT/visualization
*/

class AudioBuffer{
	private:
		const std::vector<float>* audioData;
		size_t sourcePosition;
		float* bufferData;
		PaUtilRingBuffer ringBuffer;
		
	public:
		//constructor
		AudioBuffer(int bufferSizeInSamples, const AudioLoader& loader);
		//destructor
		~AudioBuffer();
		//fill ring buffer from file data
		bool fillBuffer(int samplesToWrite);
		//read data for portaudio callback or analyzer
		int readBuffer(float* output, int frameCount);
		//peek to see data without destroying it
		int peekBuffer(float* output, int frameCount);
		//check available data
		int getAvailableReadSamples() const;
		//return if ringbuffer has data or not
		bool hasData() const;
		//disable copy constructor and assignment operator, doesn't make sense with this
    	AudioBuffer(const AudioBuffer&) = delete;
    	AudioBuffer& operator=(const AudioBuffer&) = delete;
};
#endif