#include "AudioBuffer.h"

// Constructor initializes ring buffer and sets source position to start
AudioBuffer::AudioBuffer(int bufferSizeInSamples, const AudioLoader& loader){
	audioData = &loader.getAudioData();
	sourcePosition = 0;
	bufferData = new float[bufferSizeInSamples]; 	//allocates ring buffer storage
	PaUtil_InitializeRingBuffer(&ringBuffer, sizeof(float), bufferSizeInSamples, bufferData);
}

// Destructor frees ring buffer memory
AudioBuffer::~AudioBuffer(){
	delete[] bufferData;
}

bool AudioBuffer::fillBuffer(int samplesToWrite){
	
	// Stop if we reach the end of the audio
	if(sourcePosition >= audioData->size()){
		return false;
	}
	
	// Check available space in the buffer
	int freeSpace = PaUtil_GetRingBufferWriteAvailable(&ringBuffer);
	// Calculate how much data is left
	int availableData = static_cast<int>(audioData->size() - sourcePosition);
	// Determine how many samples we can actually write
	int actualSamples = std::min({samplesToWrite, availableData, freeSpace});

	// Write samples from audioData into ring buffer
	int written = PaUtil_WriteRingBuffer(&ringBuffer, audioData->data() + sourcePosition, actualSamples);
	// Advance read pointer in source audio
	sourcePosition += written;
	// Return false if we've reached the end, true if more data remains
	return sourcePosition < audioData->size();
}


int AudioBuffer::readBuffer(float* output, int frameCount){
	return PaUtil_ReadRingBuffer(&ringBuffer, output, frameCount);
}

// Copies current buffer as a temp buffer and reads from it to peek without destroying
int AudioBuffer::peekBuffer(float* output, int frameCount){
	PaUtilRingBuffer tempBuffer = ringBuffer;
	return PaUtil_ReadRingBuffer(&tempBuffer, output, frameCount);
}

int AudioBuffer::getAvailableReadSamples() const{
	return PaUtil_GetRingBufferReadAvailable(&ringBuffer);
}

bool AudioBuffer::hasData() const{
	return PaUtil_GetRingBufferReadAvailable(&ringBuffer) > 0;
}