#include "AudioLoader.h"
#include <iostream>

// Constructor initializes sfInfo.format to 0, as required by libsndfile
AudioLoader::AudioLoader(){
    sfInfo.format = 0;
}

// libsndfile is used to load audio file into memory
bool AudioLoader::loadAudioFile(const char* filename ){
    // Open audio file for reading
    SNDFILE *infile = sf_open(filename, SFM_READ, &sfInfo);
    // Check if opening the file failed
    if(!infile) {
        std::cerr << "Failed to open audio file: " << filename << "\n";
        return false;
    }

    // Resize audioData vector to fit all frames
    audioData.resize(sfInfo.frames * sfInfo.channels);
    // Read audio data as float samples into the vector 
    sf_readf_float(infile, audioData.data(), sfInfo.frames);
    // Close audio file
    sf_close(infile);

    return true;
}   