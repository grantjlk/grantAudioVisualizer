#include "AudioAnalyzer.h"

// Constructor 
AudioAnalyzer::AudioAnalyzer(AudioBuffer* buffer, int fftSize, int sampleRate, int numBuckets, float lowFreq, float highFreq) : audioBuffer(buffer), fftSize(fftSize), sampleRate(sampleRate), numBuckets(numBuckets), lowFreq(lowFreq), highFreq(highFreq), fftInput(nullptr), fftOutput(nullptr), plan(nullptr), rmsVal(0.0f), peakAmplitude(0.0f){
	// Allocate fftw arrays for real to complex transform
	fftInput = fftwf_alloc_real(fftSize);
	fftOutput = fftwf_alloc_complex(fftSize / 2 + 1);

	// Check memory allocation succeeded
	if (!fftInput || !fftOutput) {
        std::cout << "Failed to allocate FFTW arrays" << std::endl;
        return;
    }

	// Create FFTW execution plan using FFTW_MEASURE for optimal performance (creates fastest possible plan for repeated use, but takes longer for setup)
	plan = fftwf_plan_dft_r2c_1d(fftSize, fftInput, fftOutput, FFTW_MEASURE);
	// Verify plan creation succeeded
	if(!plan){
        std::cout << "Failed to create fftwfplan" << std::endl;
        return;
    }

	// Initialize magnitude spectrum vector to hold FFT output magnitudes
	magnitudeSpectrum.resize(fftSize / 2 + 1);
	// Precompute Hanning window function
	_computeWindowFunction();
	// Setup log-based bucket ranges for visualizationBuckets
	_setupBuckets();

}

// Destructor, frees FFTW resources
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

// Analyzes the next block of audio data from the buffer
// True if successful, false if not
bool AudioAnalyzer::analyzeNextBlock(){
	if(!fftInput || !fftOutput || !plan ){
		return false;
	}
	// Peek samples from buffer (non destructive)
	int samplesRead = audioBuffer->peekBuffer(fftInput, fftSize);
	// Make sure enough samples were read
	if(samplesRead < fftSize){
		return false;
	}
	// Compute RMS and peak amplitude on raw data
	_computeRmsAndPeak();
	// Apply Hanning window for FFT
	_applyWindowFunction();
	// Execute FFT plan
	fftwf_execute(plan);
	// Convert complex FFT output to real magnitudes
	_convertOutputToMagnitudes();
	// Convert magnitudes to 32 buckets for visualization
	_computeBuckets();

	return true;
} 

// Precompute Hanning window coefficients
void AudioAnalyzer::_computeWindowFunction(){
	windowFunction.resize(fftSize);
	for (int i = 0; i < fftSize; i++) {
    	windowFunction[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (fftSize - 1)));
	}
}

// Apply Hanning window to input samples (to be done before FFT)
void AudioAnalyzer::_applyWindowFunction(){
	for(int i = 0; i < fftSize; i++){
		fftInput[i] *= windowFunction[i];
	}
}

// Compute RMS and peak amplitude of current block (to be done before FFT)
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

// Convert FFT complex output into magnitudes
// magnitude = sqrt(real^2 + imag^2) / fftSize
void AudioAnalyzer::_convertOutputToMagnitudes(){
	int numBins = fftSize / 2 + 1;

	for(int i = 0; i < numBins; i++){
		float real = fftOutput[i][0];
		float imag = fftOutput[i][1];
		magnitudeSpectrum[i] = sqrtf(real * real + imag * imag) * (1.0f / fftSize);
	}
}

// Setup frequency bucket ranges (currently hardcoded for 32 buckets, 20Hz–16kHz)
// In future, change this so we can choose buckets?
void AudioAnalyzer::_setupBuckets(){
	numBuckets = 32;
	//hardcoded using logarithmic scaling
	bucketRanges = {{0,0},{0,0},{0,0},{0,1},{1,1},{1,1},{1,1},{1,2},{2,3},{3,3},{3,4},{4,5},{5,7},{7,8},{8,10},{10,13},{13,16},{16,19},{19,24},{24,30},{30,37},{37,45},{45,56},{56,69},{69,86},{86,106},{106,130},{130,161},{161,198},{198,244},{244,301},{301,371}};

	visualizationBuckets.resize(numBuckets);
}

// Collapse FFT magnitudes into log spaced buckets
void AudioAnalyzer::_computeBuckets(){
	// Compute average magnitude in each range of bins, then set corresponding bucket to that average
	for(int i = 0; i < numBuckets; i ++){
		float sum = 0.0f;
		int startBin = bucketRanges[i].first;
		int endBin = bucketRanges[i].second;

		for(int j = startBin; j <= endBin; j++){
			sum += magnitudeSpectrum[j];
		}
		// Make the corresponding bucket the average
		int binCount = endBin - startBin + 1;
		visualizationBuckets[i] = sum / binCount;
	}
}