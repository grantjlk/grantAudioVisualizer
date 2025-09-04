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
		int numBuckets;
		float lowFreq;
		float highFreq;
		//FFT setup
		int fftSize;
		float* fftInput;
		fftwf_complex* fftOutput;
		fftwf_plan plan;

		//hann window function so its smoother
		std::vector<float> windowFunction;

		//analysis results(fft bins for freqs, rms val(loudness), peakamplitude val)
		std::vector<float> magnitudeSpectrum;
		std::vector<std::pair<int, int>> bucketRanges;
		std::vector<float> visualizationBuckets;
		float rmsVal;
		float peakAmplitude;

		void _computeWindowFunction();
		void _applyWindowFunction();	
		void _computeRmsAndPeak();
		void _convertOutputToMagnitudes();
		void _setupBuckets();
		void _computeBuckets();

	public:
		//constructor & destructor
		AudioAnalyzer(AudioBuffer* buffer, int fftSize, int sampleRate, int numBuckets = 32, float lowFreq = 20.0f, float highFreq = 16000.0f);
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