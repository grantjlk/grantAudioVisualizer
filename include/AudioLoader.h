#ifndef AUDIO_LOADER_H
#define AUDIO_LOADER_H

#include <vector>
#include <sndfile.h>

/**
 * @class AudioLoader
 * @brief Loads audio files into memory using libsndfile
 */
class AudioLoader{
    private:
        std::vector<float> audioData;   // Holds raw audio samples
        SF_INFO sfInfo;                 // Contains data about the audio file (most important is sample rate and channels)

    public:
        /// @brief Constructor initializes internal SF_INFO struct
        AudioLoader();

        /// @brief Loads audio file into memory using libsndfile
        /// @param filename Path to the audio file
        /// @return True if the file was successfully loaded, false if not
        bool loadAudioFile(const char* filename);

        /// @brief Gets a const reference of audio data, used in AudioBuffer class to have the data
        /// @return Reference to a vector of audio samples
        const std::vector<float>& getAudioData() const { return audioData;}
        /// @brief Returns the sample rate of loaded audio, used as parameter for AudioOutput and AudioAnalyzer constructors
        /// @return Sample rate (Hz)
        int getSampleRate() const { return sfInfo.samplerate; }
        /// @brief Returns channels in audio file, used in AudioOutput constructor
        /// @return Number of channels (1 = mono, 2 = stereo, etc.)
        int getChannels() const { return sfInfo.channels; }
        int getTotalFrames() const { return static_cast<int>(sfInfo.frames); }
        double getDuration() const { return static_cast<double>(sfInfo.frames) / sfInfo.samplerate; }
};

#endif