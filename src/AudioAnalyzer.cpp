#define _USE_MATH_DEFINES
#include <cmath>
#include <iostream>
#include <vector>
#include <fftw3.h>

#include "AudioBuffer.h"


class AudioAnalyzer{
	private:
		AudioBuffer* audioBuffer;

		//FFT setup
		int fftSize;
		float* fftInput;
		fftwf_complex* fftOutput;
		fftwf_plan plan;

		//hann window function so its smoother
		std::vector<float> windowFunction;

		//analysis results(fft bins for freqs, rms val, peakamplitude val)
		std::vector<float> magnitudeSpectrum;
		//maybe add a different bucket size? based on freqs in magnitude spectrum? often on a logarithmic scale
		float rmsVal;
		float peakAmplitude;

		void _computeWindowFunction();
		void _applyWindowFunction();
	public:
		//constructor & destructor
		//constructor creates a plan, sets audio buffer
		AudioAnalyzer(AudioBuffer* buffer, int fftSize);
		~AudioAnalyzer();

		//perform one analysis step
		bool analyzeNextBlock();

		//get analysis results
		const std::vector<float>& getSpectrum() const {return magnitudeSpectrum;}
		float getRmsVal() const {return rmsVal;};
		float getPeakAmplitude() const {return peakAmplitude;};

		//disable copying/ assignment operator
		AudioAnalyzer(const AudioAnalyzer&) = delete;
		AudioAnalyzer& operator=(const AudioAnalyzer&) = delete;

};

AudioAnalyzer::AudioAnalyzer(AudioBuffer* buffer, int fftSize) : audioBuffer(buffer), fftSize(fftSize), fftInput(nullptr), fftOutput(nullptr), plan(nullptr), rmsVal(0.0f), peakAmplitude(0.0f){
	//allocate fftw arrays
	fftInput = fftwf_alloc_real(fftSize);
		//2.3 fftw docs
	fftOutput = fftwf_alloc_complex(fftSize / 2 + 1);
	//check they're allocated
	if (!fftInput || !fftOutput) {
        std::cout << "Failed to allocate FFTW arrays" << std::endl;
        return;
    }

	//create plan, with measure as it will be running the same sized routine many times
	plan = fftwf_plan_dft_r2c_1d(fftSize, fftInput, fftOutput, FFTW_MEASURE);
	//check if worked
	if(!plan){
        std::cout << "Failed to create fftwfplan" << std::endl;
        return;
    }

	//initialize magnitude vector 
	windowFunction.resize(fftSize / 2 + 1);
	// then compute Hanning function
	_computeWindowFunction();


}

AudioAnalyzer::~AudioAnalyzer(){
	if(plan){
		fftwf_destroy_plan(plan);
	}
	if(fftInput){
		fftwf_free(fftInput);
	}
	if(fftOutput){
		fftwf_free(fftOutput);
	}
}

bool AudioAnalyzer::analyzeNextBlock(){
	return true;
} 

void AudioAnalyzer::_computeWindowFunction(){
	for (int i = 0; i < fftSize; i++) {
    	windowFunction[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (fftSize - 1)));
	}
}

void AudioAnalyzer::_applyWindowFunction(){
	for(int i = 0; i < fftSize; i++){
		magnitudeSpectrum[i] *= windowFunction[i];
	}
}