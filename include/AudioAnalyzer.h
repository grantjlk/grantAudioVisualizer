#define _USE_MATH_DEFINES
#include <cmath>
#include <iostream>
#include <vector>
#include <fftw3.h>
#include "AudioBuffer.h"

/**
 * @class AudioAnalyzer
 * @brief Performs real-time audio analysis using FFT to extract frequency information and audio metrics.
 * 
 * This class uses FFTW library to analyze audio data from an AudioBuffer.
 * It computes frequency spectrum, organizes frequencies into logarithmic buckets
 * for visualization, and calculates RMS and peak amplitude values.
 */
class AudioAnalyzer{
	private:
		AudioBuffer* audioBuffer;			///< Pointer to shared AudioBuffer for audio data

		int sampleRate;						///< Sample rate of audio data
		int numBuckets;						///< Number of frequency buckets for visualization (locked at 32 right now)
		float lowFreq;						///< Lowest freq to analyze (locked at 20 right now)
		float highFreq;						///< Highest freq to analyze (locked at 16k right now)

		// FFT setup
		int fftSize;						///< Number of samples to analyze
		float* fftInput;					///< FFTW input buffer (real values)
		fftwf_complex* fftOutput;			///< FFTW output buffer (complex data)
		fftwf_plan plan;					///< FFTW execution plan for repeated transforms
		std::vector<float> windowFunction;	///< Precomputed Hanning window coefficiants to reduce spectral leakage in FFT

		// Analysis results (fft bins for freqs, rms val(loudness), peak amplitude value)
		std::vector<float> magnitudeSpectrum;	///< Magnitude spectrum from FFT (frequency bin amplitudes)
		std::vector<std::pair<int, int>> bucketRanges;	///< Start and end bin indices for each visualization bucket
		std::vector<float> visualizationBuckets;	///< Logarithmically spaced frequency buckets for visualization
		float rmsVal;						///< Root mean square of current analysis block (measures loudness)
		float peakAmplitude;				///< Peak amplitude in current analysis block

		/// @brief Precomputes Hanning window coefficients
		void _computeWindowFunction();
		/// @brief Applies Hanning window to input data to reduce spectral leakage, used right before the FFT
		void _applyWindowFunction();
		/// @brief Compute root mean square and peak amplitude of current analysis block
		void _computeRmsAndPeak();
		/// @brief Converts complex FFT output to their magnitudes
		void _convertOutputToMagnitudes();
		/// @brief Sets up logarithmic frequency bucket ranges for visualization
		void _setupBuckets();
		/// @brief Computes average magnitude for each visualization bucket
		void _computeBuckets();

	public:
		/// @brief Constructor initializes FFTW, allocates buffers, and sets up analysis parameters
		/// @param buffer Pointer to AudioBuffer to analyze data from
		/// @param fftSize Number of samples per FFT
		/// @param sampleRate Audio sample rate, input from AudioLoader's sample rate
		/// @param numBuckets Number of visualization buckets (only 32 works right now)
		/// @param lowFreq Lower frequency bound for bucket grouping (only 20 works right now)
		/// @param highFreq Higher frequency bound for bucket grouping (only 16k works right now)
		AudioAnalyzer(AudioBuffer* buffer, int fftSize, int sampleRate, int numBuckets = 32, float lowFreq = 20.0f, float highFreq = 16000.0f);
		/// @brief Cleans up FFTW resources
		~AudioAnalyzer();

		/// @brief Peeks data from the next block in the AudioBuffer, applies windowing, performs FFT, computes magnitude and buckets (non-destructive)
		/// @return True if analysis was successful, false if not enough data available
		bool analyzeNextBlock();

		//Getters for analysis results
		/// @brief Gets the full magnitude spectrum from FFT analysis
		/// @return Const reference to magnitude spectrum vector
		/// @note maybe remove? keeping in case the full magnitude spectrum might be useful
		const std::vector<float>& getSpectrum() const {return magnitudeSpectrum;}

		/// @brief Gets log-spaced frequency buckets
		/// @return Reference to a vector of averaged amplitudes in different buckets
		const std::vector<float>& getBuckets() const {return visualizationBuckets;}

		/// @brief Get RMS value of most recent analysis block
		/// @return Root mean square amplitude (loudness)
		float getRmsVal() const {return rmsVal;};
		/// @brief Get peak ampitude of most recent analysis block
		/// @return Maximum amplitude in block
		float getPeakAmplitude() const {return peakAmplitude;};


		// Disable copy constructor and assignment operator
		AudioAnalyzer(const AudioAnalyzer&) = delete;
		AudioAnalyzer& operator=(const AudioAnalyzer&) = delete;

};