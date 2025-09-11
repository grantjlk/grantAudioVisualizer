#include "AudioOutput.h"

//Constructor
AudioOutput::AudioOutput(AudioBuffer* buffer, int sampleRate, int channels) 
						: stream(nullptr), audioBuffer(buffer), sampleRate(sampleRate), channels(channels)
{
	PaError err = Pa_Initialize();
	if(err != paNoError){
		printf(  "Failed to initialize PortAudio. PortAudio error: %s\n", Pa_GetErrorText( err ) );
		return;
	}
	
	err = Pa_OpenDefaultStream(&stream,
									0,          // no input channels
									channels,   // Number of output channels
									paFloat32,  // 32 bit floating point output
									sampleRate,
									256,        // frames per buffer (can adjust)
									outputCallback, // callback function
									this);      // pass this object as data

	if(err != paNoError){
		printf(  "Failed to open PortAudio output stream. PortAudio error: %s\n", Pa_GetErrorText( err ) );
		stream = nullptr;
	}

}

//destructor, stops and closes stream, then shuts down portaudio.
AudioOutput::~AudioOutput(){
	if(stream){
		Pa_StopStream(stream);
		Pa_CloseStream(stream);
		//maybe don't need?
		stream = nullptr;
	}

	Pa_Terminate();
}

// Static callback function used by PortAudio to request audio data
int AudioOutput::outputCallback( const void *inputBuffer, void *outputBuffer,
						unsigned long framesPerBuffer,
						const PaStreamCallbackTimeInfo* timeInfo,
						PaStreamCallbackFlags statusFlags,
						void *userData )
{
	//Casts userdata back into AudioOutput*
	AudioOutput* self = static_cast<AudioOutput*>(userData);
	float* out = static_cast<float*>(outputBuffer);
	
	//Attempt to read samples
	int samplesRequested = framesPerBuffer * self->channels;
	int samplesRead = self->audioBuffer->readBuffer(out, samplesRequested);
	
	//If not enough samples were read, fill the rest with silence
	for(int i = samplesRead; i < framesPerBuffer * self->channels; i++){
		out[i] = 0.0f;
	}
	// Returns paContinue to keep the stream running
	// Rather than returning paComplete, we have to stop the stream externally
	return paContinue;
}

// Starts audio playback
bool AudioOutput::start(){
	if(!stream) return false;

	PaError err = Pa_StartStream(stream);
	if(err != paNoError){
		printf(  "Failed to start PortAudio stream. PortAudio error: %s\n", Pa_GetErrorText(err) );
		return false;
	}

	return true;
}

//Stops audio playback
bool AudioOutput::stop(){
	if(!stream) return false;
	PaError err = Pa_StopStream(stream);
	if(err != paNoError){
		printf(  "Failed to stop PortAudio stream. PortAudio error: %s\n", Pa_GetErrorText(err) );
		return false;
	}

	return true;
}

// Returns true if stream is playing
bool AudioOutput::isActive() const{
	if (!stream) return false;
	return Pa_IsStreamActive(stream) == 1;
}

