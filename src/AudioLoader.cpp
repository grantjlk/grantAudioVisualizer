#include "AudioLoader.h"
#include <iostream>
    
AudioLoader::AudioLoader(){
    //needs to be set to 0
    sfInfo.format = 0;
}

bool AudioLoader::loadAudioFile(const char* filename ){
    SNDFILE *infile = sf_open(filename, SFM_READ, &sfInfo);
    //if file failed to open, return false
    if(!infile) {
        std::cerr << "Failed to open audio file: " << filename << "\n";
        return false;
    }
    //resize audioData vec to fit all frames
    audioData.resize(sfInfo.frames * sfInfo.channels);
    //read frames into audio data
    sf_readf_float(infile, audioData.data(), sfInfo.frames);
    //close file
    sf_close(infile);

    return true;
}