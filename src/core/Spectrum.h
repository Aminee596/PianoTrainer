#pragma once

#include <vector>

namespace piano
{
struct Spectrum
{
    struct Peak { float hz; float amp; };

    int size = 0;
    double rate = 48000.0;
    std::vector<float> power;
    std::vector<Peak> peaks;

    void prepare (int n, double sampleRate);

    void run (const float* samples, int available, bool findPeaks = true);

    std::vector<Peak> peaksOf (const std::vector<float>& powerSpectrum) const;

    float amplitudeAt (double hz) const { return amplitudeAt (peaks, hz); }
    float amplitudeAt (const std::vector<Peak>& list, double hz) const;
    int   peakIndex (const std::vector<Peak>& list, double hz, double minBins = 0.25) const;
    float amplitudeNear (const std::vector<Peak>& list, double hz, double minBins) const;

    void bandBins (double hz, int& first, int& last, double minBins = 0.6) const;

    float bandAmplitude (double hz, double minBins = 0.6) const { return bandAmplitude (power, hz, minBins); }
    float bandAmplitude (const std::vector<float>& savedPower, double hz, double minBins = 0.6) const;

    float medianAmplitude (double loHz, double hiHz) const;

private:
    std::vector<float> window, re, im;
    std::vector<int> bitrev;
    std::vector<float> cosTable, sinTable;
};

}
