#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

#include <portaudio.h>
#include <atomic>
#include "AudioBuffer.h"

/**
 * @class AudioOutput
 * @brief Handles real-time audio output using PortAudio and an AudioBuffer.
 */
class AudioOutput{
	private:
		PaStream* stream;			///< Portaudio stream object
		AudioBuffer* audioBuffer;	///< Pointer to shared audiobuffer
		int sampleRate;				///< Sample rate of audio info from portaudio
		int channels;				///< Number of audio channels (eg. 2 for stereo)
		std::atomic<int> underrunCount;			///< Callbacks where the ring buffer didn't have enough samples (heard as a dropout)
		std::atomic<int> deviceUnderflowCount;	///< Callbacks where portaudio says the device itself ran dry (callback ran late)

		/// @brief Static callback function required by PortAudio.
		/// Pulls audio data from AudioBuffer and writes to the outputBuffer
		static int outputCallback( const void *inputBuffer, void *outputBuffer,
						unsigned long framesPerBuffer,
						const PaStreamCallbackTimeInfo* timeInfo,
						PaStreamCallbackFlags statusFlags,
						void *userData );
	public:
		/// @brief Constructor initializes PortAudio and sets up the output stream to default output device.
		/// @param buffer Pointer to the AudioBuffer used as the data source
		/// @param sampleRate Sample rate of audio, input from AudioLoader's sample rate
		/// @param channels Number of output channels from audio, input from AudioLoader's channels
		AudioOutput(AudioBuffer* buffer, int sampleRate, int channels);

		/// @brief Destructor stops and closes the stream, and terminates PortAudio
		~AudioOutput();


		/// @brief Starts audio playback
		/// @return True on success, false on failure
		bool start();

		/// @brief Stops audio playback
		/// @return True on success, false on failure
		bool stop();

		/// @brief Checks if audio stream is currently active, ((Often used to run a while loop))
		/// @return True if active, false otherwise
		bool isActive() const;

		/// @brief Gets how many callbacks came up short on samples since start, safe to call from any thread
		/// @return Total buffer underruns
		int getUnderrunCount() const {return underrunCount.load(std::memory_order_relaxed);}

		/// @brief Gets how many times portaudio reported the output device ran dry, safe to call from any thread
		/// @return Total device underflows
		int getDeviceUnderflowCount() const {return deviceUnderflowCount.load(std::memory_order_relaxed);}

		// Disable copy constructor and assignment operator
		AudioOutput(const AudioOutput&) = delete;
		AudioOutput& operator=(const AudioOutput&) = delete;
};

#endif