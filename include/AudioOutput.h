#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H
#include <portaudio.h>
#include "AudioBuffer.h"
class AudioOutput{
	private:
		PaStream* stream;
		AudioBuffer* audioBuffer;
		int sampleRate;
		int channels;
		//portaudio requires a static callback
		static int outputCallback( const void *inputBuffer, void *outputBuffer,
						unsigned long framesPerBuffer,
						const PaStreamCallbackTimeInfo* timeInfo,
						PaStreamCallbackFlags statusFlags,
						void *userData );
	public:
		AudioOutput(AudioBuffer* buffer, int sampleRate, int channels);
		~AudioOutput();

		bool start();
		bool stop();
		bool isActive() const;

		// Disable copying
		AudioOutput(const AudioOutput&) = delete;
		AudioOutput& operator=(const AudioOutput&) = delete;
};

#endif