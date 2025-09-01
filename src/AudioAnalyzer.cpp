#define _USE_MATH_DEFINES
#include <cmath>
#include <iostream>
#include <vector>
#include <fftw3.h>

#include "AudioBuffer.h"


class AudioAnalyzer{
	private:
		AudioBuffer* audioBuffer;
		int sampleRate;
		//FFT setup
		int fftSize;
		float* fftInput;
		fftwf_complex* fftOutput;
		fftwf_plan plan;

		//hann window function so its smoother
		std::vector<float> windowFunction;

		//analysis results(fft bins for freqs, rms val(loudness), peakamplitude val)
		std::vector<float> magnitudeSpectrum;
		std::vector<float> visualizationBuckets;
		float rmsVal;
		float peakAmplitude;

		void _computeWindowFunction();
		void _applyWindowFunction();	
		void _computeRmsAndPeak();
		void _convertOutputToMagnitudes();
			//maybe add an int?
		void _computeVisualizationBuckets();

	public:
		//constructor & destructor
		//constructor creates a plan, sets audio buffer
		AudioAnalyzer(AudioBuffer* buffer, int fftSize, int sampleRate);
		~AudioAnalyzer();

		//perform one analysis step
		bool analyzeNextBlock();

		//get analysis results
		//maybe remove getspectrum?
		const std::vector<float>& getSpectrum() const {return magnitudeSpectrum;}
		const std::vector<float>& getBuckets() const {return visualizationBuckets;}
		float getRmsVal() const {return rmsVal;};
		float getPeakAmplitude() const {return peakAmplitude;};


		//disable copying/ assignment operator
		AudioAnalyzer(const AudioAnalyzer&) = delete;
		AudioAnalyzer& operator=(const AudioAnalyzer&) = delete;

};

AudioAnalyzer::AudioAnalyzer(AudioBuffer* buffer, int fftSize, int sampleRate) : audioBuffer(buffer), fftSize(fftSize), sampleRate(sampleRate),fftInput(nullptr), fftOutput(nullptr), plan(nullptr), rmsVal(0.0f), peakAmplitude(0.0f){
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
	magnitudeSpectrum.resize(fftSize / 2 + 1);
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
//TODO:
//IMPLEMENT ANALYZE NEXT BLCOK, return true if next block successfully analyzed, false if no
//apply windowing function, execute fft plan, convert output to magnitudes, possibly move these into buckets, based on logarithmic scale?
//calculate rmsval and peakamplitude
bool AudioAnalyzer::analyzeNextBlock(){
	//if any of these things don't exist, return false
	if(!fftInput || !fftOutput || !plan ){
		return false;
	}

	int samplesRead = audioBuffer->readBuffer(fftInput, fftSize);
	//if not enough samples read, return false
	if(samplesRead < fftSize){
		return false;
	}
	//compute rms and peak amplitude on data
	_computeRmsAndPeak();
	//apply window function for fft
	_applyWindowFunction();
	//execute plan
	fftwf_execute(plan);
	//convert fft data to magnitudes
	_convertOutputToMagnitudes();
	//convert magnitudes to buckets?
	_computeVisualizationBuckets();

	return true;
} 

void AudioAnalyzer::_computeWindowFunction(){
	windowFunction.resize(fftSize);
	for (int i = 0; i < fftSize; i++) {
    	windowFunction[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (fftSize - 1)));
	}
}

void AudioAnalyzer::_applyWindowFunction(){
	for(int i = 0; i < fftSize; i++){
		fftInput[i] *= windowFunction[i];
	}
}

void AudioAnalyzer::_computeRmsAndPeak(){
	float sumSquares = 0.0f;
	float peak = 0.0f;

	for(int i = 0; i < fftSize; i++){
		float sample = fftInput[i];

		sumSquares += sample * sample;

		if(fabsf(sample) > peak){
			peak = fabsf(sample);
		}
	}
	rmsVal = sqrtf(sumSquares / fftSize);
	peakAmplitude = peak;
}
	//to get magnitude of a fft, take the sqrt(real*real + imag*imag)
	//then normalize by num of samples (fftSize)
void AudioAnalyzer::_convertOutputToMagnitudes(){
	int numBins = fftSize / 2 + 1;

	for(int i = 0; i < numBins; i++){
		float real = fftOutput[i][0];
		float imag = fftOutput[i][1];
		//also normalize
		magnitudeSpectrum[i] = sqrtf(real * real + imag * imag) * (1.0f / fftSize);
	}
}
//20hz to 16000hz
void AudioAnalyzer::_computeVisualizationBuckets(){

}