#include "AudioOutput.h"
#include <algorithm>
#include <cstdio>

// frames per callback we ask portaudio for. the mono scratch is sized off this
static const int kFramesPerBuffer = 256;

// Constructor
AudioOutput::AudioOutput(AudioBuffer* buffer, SpscRingBuffer<float>* playedAudio, int sampleRate, int channels)
						: stream(nullptr), paInitialized(false), audioBuffer(buffer), playedAudio(playedAudio), monoScratch(kFramesPerBuffer),
						sampleRate(sampleRate), channels(channels), underrunCount(0), deviceUnderflowCount(0)
{
	PaError err = Pa_Initialize();
	if(err != paNoError){
		printf(  "Failed to initialize PortAudio. PortAudio error: %s\n", Pa_GetErrorText( err ) );
		return;
	}
	// only terminate in the destructor if this worked, portaudio counts initialize/terminate calls
	paInitialized = true;

	err = Pa_OpenDefaultStream(&stream,
									0,          // no input channels
									channels,   // Number of output channels
									paFloat32,  // 32 bit floating point output
									sampleRate,
									kFramesPerBuffer,	// frames per buffer (can adjust)
									outputCallback, // callback function
									this);      // pass this object as data

	if(err != paNoError){
		printf(  "Failed to open PortAudio output stream. PortAudio error: %s\n", Pa_GetErrorText( err ) );
		stream = nullptr;
	}

}

// Destructor, stops and closes stream, then shuts down portaudio.
AudioOutput::~AudioOutput(){
	if(stream){
		Pa_StopStream(stream);
		Pa_CloseStream(stream);
		//maybe don't need?
		stream = nullptr;
	}

	if(paInitialized){
		Pa_Terminate();
	}
}

// Static callback function used by PortAudio to request audio data
// (input and timing info aren't used, names commented out so the compiler doesn't warn)
int AudioOutput::outputCallback( const void* /*inputBuffer*/, void *outputBuffer,
						unsigned long framesPerBuffer,
						const PaStreamCallbackTimeInfo* /*timeInfo*/,
						PaStreamCallbackFlags statusFlags,
						void *userData )
{
	//Casts userdata back into AudioOutput*
	AudioOutput* self = static_cast<AudioOutput*>(userData);
	float* out = static_cast<float*>(outputBuffer);
	
	//Attempt to read samples
	int samplesRequested = static_cast<int>(framesPerBuffer) * self->channels;
	int samplesRead = self->audioBuffer->readBuffer(out, samplesRequested);
	
	//If not enough samples were read, fill the rest with silence
	for(int i = samplesRead; i < samplesRequested; i++){
		out[i] = 0.0f;
	}

	// count underruns for perf stats. this runs on portaudio's real time thread, so no locks or
	// allocations allowed here - an atomic fetch_add is lock free so it's safe
	if(samplesRead < samplesRequested){
		self->underrunCount.fetch_add(1, std::memory_order_relaxed);
	}
	if(statusFlags & paOutputUnderflow){
		self->deviceUnderflowCount.fetch_add(1, std::memory_order_relaxed);
	}

	// send a mono copy of what was just played to the analyzer, so the bars match what's being heard.
	// the analyzer used to peek ahead in audioBuffer from a third thread, which raced with this callback
	// and the fill thread, and showed audio that hadn't played yet
	if(self->playedAudio){
		// chunks of the scratch size, in case portaudio ever hands us more frames than we asked for
		for(unsigned long done = 0; done < framesPerBuffer; ){
			unsigned long frames = std::min(framesPerBuffer - done, static_cast<unsigned long>(self->monoScratch.size()));
			// average the channels, interleaved stereo fed straight into the fft reads as an octave too low
			for(unsigned long f = 0; f < frames; f++){
				float sum = 0.0f;
				for(int c = 0; c < self->channels; c++){
					sum += out[(done + f) * self->channels + c];
				}
				self->monoScratch[f] = sum / self->channels;
			}
			// if the analyzer fell behind (eg. render thread stalled) this just drops what doesn't fit,
			// playback never waits on the visuals
			self->playedAudio->write(self->monoScratch.data(), frames);
			done += frames;
		}
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
	// already stopped (render thread stops it when playback ends, then main stops it again on exit)
	if(Pa_IsStreamStopped(stream) == 1) return true;
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

