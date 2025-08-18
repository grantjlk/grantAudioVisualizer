#include "AudioBuffer.h"

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