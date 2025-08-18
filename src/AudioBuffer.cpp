#include <vector>
#include <cstddef>
#include <algorithm>

#include "pa_ringbuffer.h"
#include "AudioLoader.h"


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
		//check available data
		int getAvailableReadSamples() const;
		//return if ringbuffer has data or not
		bool hasData() const;
		//disable copy constructor and assignment operator, doesn't make sense with this
    	AudioBuffer(const AudioBuffer&) = delete;
    	AudioBuffer& operator=(const AudioBuffer&) = delete;
};

AudioBuffer::AudioBuffer(int bufferSizeInSamples, const AudioLoader& loader){
	audioData = &loader.getAudioData();
	sourcePosition = 0;
	bufferData = new float[bufferSizeInSamples];
	PaUtil_InitializeRingBuffer(&ringBuffer, sizeof(float), bufferSizeInSamples, bufferData);
}

AudioBuffer::~AudioBuffer(){
	delete[] bufferData;
}

bool AudioBuffer::fillBuffer(int samplesToWrite){
	
	//check if we reached end of audio file
	if(sourcePosition >= audioData->size()){
		return false;
	}
	
	
	//calculate freespace left
	int freeSpace = PaUtil_GetRingBufferWriteAvailable(&ringBuffer);
	//calculate how much data is left
	int availableData = static_cast<int>(audioData->size() - sourcePosition);
	// check how much we can actually write
	int actualSamples = std::min({samplesToWrite, availableData, freeSpace});

	//write data to ring buffer
	int written = PaUtil_WriteRingBuffer(&ringBuffer, audioData->data() + sourcePosition, actualSamples);
	//move position forward by what we wrote
	sourcePosition += written;
	//return false if we've reached the end, true if more data remains
	return sourcePosition < audioData->size();
}

int AudioBuffer::readBuffer(float* output, int frameCount){
	return PaUtil_ReadRingBuffer(&ringBuffer, output, frameCount);
}

int AudioBuffer::getAvailableReadSamples() const{
	return PaUtil_GetRingBufferReadAvailable(&ringBuffer);
}

bool AudioBuffer::hasData() const{
	return PaUtil_GetRingBufferReadAvailable(&ringBuffer) > 0;
}