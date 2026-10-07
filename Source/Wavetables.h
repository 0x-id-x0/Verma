#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <complex>
#include <cmath>
#include <cstdint>

// Verma by 0x.id : band-limited, mipmapped wavetable bank (built with an inverse FFT)
namespace verma
{
constexpr int kTableSize    = 2048;
constexpr int kFftOrder     = 11;
constexpr int kFrames       = 32;
constexpr int kMipLevels    = 11;   // level L keeps harmonics <= 1024 >> L
constexpr int kMaxHarmonics = 1023;
constexpr int kNumTables    = 6;

inline juce::StringArray getWavetableNames()
{
    return { "Basic Shapes", "Harmonic Sweep", "PWM", "Vocal Formant", "Hollow Fold", "Digital Grit" };
}

class WavetableBank
{
public:
    WavetableBank() { build(); }

    const float* getFrame (int table, int mip, int frame) const noexcept
    {
        return data.data() + offset (table, mip, frame);
    }

    static int mipForFrequency (double freq, double sampleRate) noexcept
    {
        const double maxHarm = (sampleRate * 0.5) / juce::jmax (1.0, freq);
        for (int l = 0; l < kMipLevels; ++l)
            if ((double) (1024 >> l) <= maxHarm)
                return l;
        return kMipLevels - 1;
    }

private:
    std::vector<float> data;

    static size_t offset (int table, int mip, int frame) noexcept
    {
        const size_t idx = ((size_t) table * kMipLevels + (size_t) mip) * kFrames + (size_t) frame;
        return idx * (size_t) (kTableSize + 1);
    }

    static float hash01 (int n) noexcept
    {
        uint32_t x = (uint32_t) n * 747796405u + 2891336453u;
        x = ((x >> ((x >> 28u) + 4u)) ^ x) * 277803737u;
        x = (x >> 22u) ^ x;
        return (float) (x & 0xffffffu) / (float) 0xffffffu;
    }

    static float gauss (float x, float c, float w) noexcept
    {
        const float d = (x - c) / w;
        return std::exp (-0.5f * d * d);
    }

    // Fill cosine (ca) and sine (sa) amplitudes for harmonics 1..kMaxHarmonics at morph position t (0..1)
    static void spectrum (int table, float t, std::vector<float>& ca, std::vector<float>& sa)
    {
        std::fill (ca.begin(), ca.end(), 0.0f);
        std::fill (sa.begin(), sa.end(), 0.0f);
        const float pi = juce::MathConstants<float>::pi;

        switch (table)
        {
            case 0: // Basic Shapes: sine -> triangle -> saw -> square
            {
                auto shapeAmp = [pi] (int s, int h) -> float
                {
                    switch (s)
                    {
                        case 0:  return h == 1 ? 1.0f : 0.0f;
                        case 1:  return (h % 2 == 1) ? (8.0f / (pi * pi)) * ((((h - 1) / 2) % 2 == 0) ? 1.0f : -1.0f) / (float) (h * h) : 0.0f;
                        case 2:  return (2.0f / pi) / (float) h;
                        default: return (h % 2 == 1) ? (4.0f / pi) / (float) h : 0.0f;
                    }
                };
                const float x = t * 3.0f;
                const int seg = juce::jmin ((int) x, 2);
                const float f = x - (float) seg;
                for (int h = 1; h <= kMaxHarmonics; ++h)
                    sa[(size_t) h] = shapeAmp (seg, h) * (1.0f - f) + shapeAmp (seg + 1, h) * f;
                break;
            }
            case 1: // Harmonic Sweep: resonant peak travelling up the spectrum
            {
                const float centre = 1.0f + t * 40.0f;
                const float width  = 1.5f + t * 5.0f;
                for (int h = 1; h <= kMaxHarmonics; ++h)
                {
                    const float fh = (float) h;
                    float a = 0.15f / fh + gauss (fh, centre, width) / (1.0f + 0.03f * fh);
                    if (h == 1) a += 0.5f;
                    sa[(size_t) h] = a;
                }
                break;
            }
            case 2: // PWM: square -> thin pulse
            {
                const float w = 0.5f - 0.47f * t;
                for (int h = 1; h <= kMaxHarmonics; ++h)
                    ca[(size_t) h] = 2.0f * std::sin (pi * (float) h * w) / (pi * (float) h);
                break;
            }
            case 3: // Vocal Formant: A -> E -> I -> O -> U
            {
                static const float f[5][3] = { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 350, 1700, 2700 },
                                               { 450, 800, 2830 },  { 325, 700, 2530 } };
                const float x = t * 4.0f;
                const int seg = juce::jmin ((int) x, 3);
                const float k = x - (float) seg;
                float F[3];
                for (int i = 0; i < 3; ++i)
                    F[i] = f[seg][i] * (1.0f - k) + f[seg + 1][i] * k;
                for (int h = 1; h <= kMaxHarmonics; ++h)
                {
                    const float hz = 110.0f * (float) h;
                    float a = (gauss (hz, F[0], 90.0f) + 0.6f * gauss (hz, F[1], 110.0f) + 0.3f * gauss (hz, F[2], 150.0f))
                              / std::pow ((float) h, 0.3f);
                    a += 0.03f / (float) h;
                    if (h == 1) a += 0.35f;
                    sa[(size_t) h] = a;
                }
                break;
            }
            case 4: // Hollow Fold: saw -> hollow square with comb colouring
            {
                for (int h = 1; h <= kMaxHarmonics; ++h)
                {
                    const float fh = (float) h;
                    float a = (1.0f / fh) * ((h % 2 == 1) ? 1.0f : (1.0f - t));
                    a *= 0.6f + 0.4f * std::cos (fh * t * 0.9f);
                    a *= std::pow (fh, t * 0.35f);
                    sa[(size_t) h] = a;
                }
                break;
            }
            default: // Digital Grit: deterministic random spectra, brighter + gappier with t
            {
                for (int h = 1; h <= kMaxHarmonics; ++h)
                {
                    float a = (0.25f + 0.75f * hash01 (h)) / std::pow ((float) h, 1.6f - 1.2f * t);
                    if (h > 1 && hash01 (h + 9000) < 0.35f * t) a = 0.0f;
                    if (h == 1) a = juce::jmax (a, 0.6f);
                    const float ph = hash01 (h + 5000) * 2.0f * pi;
                    ca[(size_t) h] = a * std::cos (ph);
                    sa[(size_t) h] = a * std::sin (ph);
                }
                break;
            }
        }
    }

    void build()
    {
        data.assign ((size_t) kNumTables * kMipLevels * kFrames * (kTableSize + 1), 0.0f);
        juce::dsp::FFT fft (kFftOrder);
        std::vector<std::complex<float>> in ((size_t) kTableSize), out ((size_t) kTableSize);
        std::vector<float> ca ((size_t) kMaxHarmonics + 1), sa ((size_t) kMaxHarmonics + 1);

        for (int tb = 0; tb < kNumTables; ++tb)
        {
            for (int fr = 0; fr < kFrames; ++fr)
            {
                spectrum (tb, (float) fr / (float) (kFrames - 1), ca, sa);
                float norm = 1.0f;

                for (int mip = 0; mip < kMipLevels; ++mip)
                {
                    const int limit = juce::jmin (kMaxHarmonics, 1024 >> mip);
                    std::fill (in.begin(), in.end(), std::complex<float>());

                    for (int h = 1; h <= limit; ++h)
                    {
                        const std::complex<float> c (ca[(size_t) h] * 0.5f, -sa[(size_t) h] * 0.5f);
                        in[(size_t) h] = c;
                        in[(size_t) (kTableSize - h)] = std::conj (c);
                    }

                    fft.perform (in.data(), out.data(), true);

                    float* dst = data.data() + offset (tb, mip, fr);
                    float peak = 0.0f;
                    for (int i = 0; i < kTableSize; ++i)
                    {
                        dst[i] = out[(size_t) i].real();
                        peak = juce::jmax (peak, std::abs (dst[i]));
                    }
                    if (mip == 0)
                        norm = peak > 1.0e-9f ? 1.0f / peak : 1.0f;
                    for (int i = 0; i < kTableSize; ++i)
                        dst[i] *= norm;
                    dst[kTableSize] = dst[0]; // guard sample for interpolation
                }
            }
        }
    }
};
} // namespace verma
