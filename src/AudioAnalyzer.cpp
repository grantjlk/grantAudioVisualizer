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
		float rmsVal;
		float peakAmplitude;
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

AudioAnalyzer::AudioAnalyzer(AudioBuffer* buffer, int fftSize){

}

AudioAnalyzer::~AudioAnalyzer(){

}

bool AudioAnalyzer::analyzeNextBlock(){
	return true;
} 