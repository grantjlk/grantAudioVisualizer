#include "AudioOutput.h"

//constructor
AudioOutput::AudioOutput(AudioBuffer* buffer, int sampleRate, int channels) 
						: stream(nullptr), audioBuffer(buffer), sampleRate(sampleRate), channels(channels)
{
		//possibly put this in main or somehting,
	PaError err = Pa_Initialize();
	if(err != paNoError){
		printf(  "Failed to initialize PortAudio. PortAudio error: %s\n", Pa_GetErrorText( err ) );
		return;
	}
	
	// Open default output stream
	err = Pa_OpenDefaultStream(&stream,
									0,          // no input channels
									channels,   // output channels
									paFloat32,  // 32 bit floating point output
									sampleRate,
									256,        // frames per buffer (can adjust)
									outputCallback, // callback function
									this);      // user data passed to callback
	if(err != paNoError){
		printf(  "Failed to open PortAudio output stream. PortAudio error: %s\n", Pa_GetErrorText( err ) );
		stream = nullptr;
	}

}

//destructor
AudioOutput::~AudioOutput(){
		//if stream is open 
	if(stream){
		Pa_StopStream(stream);
		Pa_CloseStream(stream);
		//maybe don't need?
		stream = nullptr;
	}
	//possibly put this in main or somehting
	Pa_Terminate();
}

int AudioOutput::outputCallback( const void *inputBuffer, void *outputBuffer,
						unsigned long framesPerBuffer,
						const PaStreamCallbackTimeInfo* timeInfo,
						PaStreamCallbackFlags statusFlags,
						void *userData )
{
	//turning userdata back into audioOutput* (c style callback)
	AudioOutput* self = static_cast<AudioOutput*>(userData);
	float* out = static_cast<float*>(outputBuffer);
				//CHANGED back to readbuffer
	int samplesRead = self->audioBuffer->readBuffer(out, framesPerBuffer * self->channels);
	
	//if we didn't get enough samples, fill the rest with silence
	for(int i = samplesRead; i < framesPerBuffer * self->channels; i++){
		out[i] = 0.0f;
	}
		//return paContinue if we read frames, and paComplete if it's the end
		//instead of stopping when we don't read frames
		//return pacontinue and continue if there are frames left to read 
		//return pacomplete if there are no more frames to read and we have exhausted the buffer
		//or deal with pacomplete outside of class, in main loop
	return paContinue;
}

	//starts the audio output, returns true if it worked, false if not
bool AudioOutput::start(){
	if(!stream) return false;
	PaError err = Pa_StartStream(stream);
	if(err != paNoError){
		printf(  "Failed to start PortAudio stream. PortAudio error: %s\n", Pa_GetErrorText(err) );
		return false;
	}

	return true;
}

bool AudioOutput::stop(){
	if(!stream) return false;
	PaError err = Pa_StopStream(stream);
	if(err != paNoError){
		printf(  "Failed to stop PortAudio stream. PortAudio error: %s\n", Pa_GetErrorText(err) );
		return false;
	}

	return true;
}

bool AudioOutput::isActive() const{
	if (!stream) return false;
	return Pa_IsStreamActive(stream) == 1;
}

