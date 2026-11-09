#include "AudioBuffer.h"

// Constructor initializes ring buffer and sets source position to start
// (the ring buffer throws on a non power of two size, portaudio's used to just fail quietly)
AudioBuffer::AudioBuffer(int bufferSizeInSamples, const AudioLoader& loader)
	: audioData(&loader.getAudioData()), sourcePosition(0), ringBuffer(static_cast<size_t>(bufferSizeInSamples))
{
}

bool AudioBuffer::fillBuffer(int samplesToWrite){

	// Stop if we reach the end of the audio
	if(sourcePosition >= audioData->size()){
		return false;
	}

	// Calculate how much data is left
	size_t availableData = audioData->size() - sourcePosition;
	// Determine how many samples to try, write() itself stops when the buffer is full
	size_t actualSamples = std::min(static_cast<size_t>(samplesToWrite), availableData);

	// Write samples from audioData into ring buffer
	size_t written = ringBuffer.write(audioData->data() + sourcePosition, actualSamples);
	// Advance read pointer in source audio
	sourcePosition += written;
	// Return false if we've reached the end, true if more data remains
	return sourcePosition < audioData->size();
}


int AudioBuffer::readBuffer(float* output, int frameCount){
	return static_cast<int>(ringBuffer.read(output, static_cast<size_t>(frameCount)));
}

int AudioBuffer::getAvailableReadSamples() const{
	return static_cast<int>(ringBuffer.readAvailable());
}

bool AudioBuffer::hasData() const{
	return ringBuffer.readAvailable() > 0;
}
