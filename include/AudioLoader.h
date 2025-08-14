#ifndef AUDIO_LOADER_H
#define AUDIO_LOADER_H

#include <vector>
#include <sndfile.h>

class AudioLoader{
    private:
        std::vector<float> audioData;
        SF_INFO sfInfo;

    public:
        AudioLoader();
        bool loadAudioFile(const char* filename);

        const std::vector<float>& getAudioData(){   return audioData;}
        int getSampleRate() const { return sfInfo.samplerate; }
        int getChannels() const { return sfInfo.channels; }
        int getTotalFrames() const { return static_cast<int>(sfInfo.frames); }
        double getDuration() const { return static_cast<double>(sfInfo.frames) / sfInfo.samplerate; }
};

#endif