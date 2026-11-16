#include "AudioAnalyzer.h"

// Constructor 
AudioAnalyzer::AudioAnalyzer(SpscRingBuffer<float>* playedAudio, int fftSize, int sampleRate, int numBuckets, float lowFreq, float highFreq) : playedAudio(playedAudio), incoming(playedAudio->getCapacity()), window(fftSize, 0.0f), samplesReceived(0), sampleRate(sampleRate), numBuckets(numBuckets), lowFreq(lowFreq), highFreq(highFreq), fftSize(fftSize), fftInput(nullptr), fftOutput(nullptr), plan(nullptr), rmsVal(0.0f), peakAmplitude(0.0f){
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
	// Take everything played since last frame (~735 samples at 60fps and 44.1kHz)
	size_t got = playedAudio->read(incoming.data(), incoming.size());
	if(got == 0){
		return false;	// nothing new played, keep showing the last result
	}

	// Slide the window forward so it always holds the latest fftSize samples
	size_t size = window.size();
	if(got >= size){
		std::copy(incoming.begin() + (got - size), incoming.begin() + got, window.begin());
	} else {
		std::copy(window.begin() + got, window.end(), window.begin());
		std::copy(incoming.begin(), incoming.begin() + got, window.end() - got);
	}
	samplesReceived += got;
	// Wait until the window has been filled at least once (right at the start of playback)
	if(samplesReceived < size){
		return false;
	}

	// fftw transforms fftInput in place, so work on a copy and keep the window intact for next frame
	std::copy(window.begin(), window.end(), fftInput);
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
    	windowFunction[i] = 0.5f * (1.0f - cosf(2.0f * static_cast<float>(M_PI) * i / (fftSize - 1)));
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

// Setup log spaced frequency bucket ranges from lowFreq to highFreq
void AudioAnalyzer::_setupBuckets(){
	int maxBin = fftSize / 2;
	float binsPerHz = static_cast<float>(fftSize) / sampleRate;
	// Can't go above half the sample rate, the fft has no bins up there
	float top = std::min(highFreq, sampleRate / 2.0f);
	float ratio = top / lowFreq;

	bucketRanges.resize(numBuckets);
	bucketCenters.resize(numBuckets);
	for(int i = 0; i < numBuckets; i++){
		// Bucket edges and center as bin positions (can be fractional, like bin 4.2)
		float lowPos = lowFreq * powf(ratio, static_cast<float>(i) / numBuckets) * binsPerHz;
		float highPos = lowFreq * powf(ratio, static_cast<float>(i + 1) / numBuckets) * binsPerHz;
		float centerPos = lowFreq * powf(ratio, (i + 0.5f) / numBuckets) * binsPerHz;

		// The bins that land inside this bucket. The low buckets are narrower than one fft bin (~43 Hz),
		// so this can come out empty (end < start), _computeBuckets handles that
		int startBin = static_cast<int>(ceilf(lowPos));
		int endBin = static_cast<int>(ceilf(highPos)) - 1;
		bucketRanges[i] = {startBin, std::min(endBin, maxBin)};
		bucketCenters[i] = std::min(centerPos, static_cast<float>(maxBin));
	}

	visualizationBuckets.resize(numBuckets);
}

// Collapse FFT magnitudes into log spaced buckets
void AudioAnalyzer::_computeBuckets(){
	for(int i = 0; i < numBuckets; i ++){
		float sum = 0.0f;
		int startBin = bucketRanges[i].first;
		int endBin = bucketRanges[i].second;

		// Not even two bins in this bucket, so blend between the two bins around its center
		// (otherwise a few bars in a row would just show the same bin)
		if(endBin <= startBin){
			int lowBin = std::min(static_cast<int>(bucketCenters[i]), fftSize / 2 - 1);
			float t = bucketCenters[i] - lowBin;
			visualizationBuckets[i] = magnitudeSpectrum[lowBin] * (1.0f - t) + magnitudeSpectrum[lowBin + 1] * t;
			continue;
		}

		// Compute average magnitude in the range of bins, then set corresponding bucket to that average
		for(int j = startBin; j <= endBin; j++){
			sum += magnitudeSpectrum[j];
		}
		// Make the corresponding bucket the average
		int binCount = endBin - startBin + 1;
		visualizationBuckets[i] = sum / binCount;
	}
}