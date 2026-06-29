#include "WavSynth.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <vector>

namespace rhythm {

namespace {

constexpr double kPi = 3.14159265358979323846;

double midiToHz(int note)
{
    return 440.0 * std::pow(2.0, (note - 69) / 12.0);
}

void putU32(std::vector<uint8_t>& b, uint32_t v)
{
    b.push_back(v & 0xff); b.push_back((v >> 8) & 0xff);
    b.push_back((v >> 16) & 0xff); b.push_back((v >> 24) & 0xff);
}
void putU16(std::vector<uint8_t>& b, uint16_t v)
{
    b.push_back(v & 0xff); b.push_back((v >> 8) & 0xff);
}

} // namespace

bool writeWavPreview(const std::string& path, const Sequence& sequence,
                     double bpm, int sampleRate)
{
    if (sampleRate <= 0) sampleRate = 44100;
    const double secsPerBeat = 60.0 / (bpm > 0 ? bpm : 120.0);

    // Total length = end of last note + release tail.
    double endBeats = 0.0;
    for (const auto& e : sequence) endBeats = std::max(endBeats, e.endBeats());
    const double tailSecs = 1.5;
    const long total = (long)((endBeats * secsPerBeat + tailSecs) * sampleRate) + 1;
    if (total <= 1) return false;

    std::vector<float> buf(total, 0.0f);

    // Simple decaying harmonic voice (electric-piano-ish): a few partials with
    // an exponential amplitude decay plus a short attack and release.
    const double partials[][2] = { {1.0, 1.0}, {2.0, 0.5}, {3.0, 0.28}, {4.0, 0.12} };
    const double attack = 0.005;   // seconds
    const double release = 0.06;   // seconds
    const double decayTau = 1.1;   // seconds (note ringing)

    for (const auto& e : sequence) {
        const double f = midiToHz(e.note);
        const long start = (long)(e.startBeats * secsPerBeat * sampleRate);
        const double noteLen = std::max(0.05, e.lengthBeats * secsPerBeat);
        const long sustain = (long)(noteLen * sampleRate);
        const long relSamples = (long)(release * sampleRate);
        const long len = sustain + relSamples;
        const float vel = std::clamp(e.velocity, 0.0f, 1.0f);

        for (long i = 0; i < len; ++i) {
            const long idx = start + i;
            if (idx < 0 || idx >= total) continue;
            const double t = (double)i / sampleRate;

            // Envelope: attack ramp, exponential decay, linear release.
            double env = std::exp(-t / decayTau);
            if (t < attack) env *= t / attack;
            if (i > sustain) {
                double r = (double)(i - sustain) / std::max(1L, relSamples);
                env *= std::max(0.0, 1.0 - r);
            }

            double s = 0.0;
            for (const auto& p : partials)
                s += p[1] * std::sin(2.0 * kPi * f * p[0] * t);
            s /= 1.9; // normalise partial sum

            buf[idx] += (float)(s * env * vel * 0.25);
        }
    }

    // Normalise to avoid clipping.
    float peak = 1e-6f;
    for (float v : buf) peak = std::max(peak, std::fabs(v));
    const float gain = (peak > 0.9f) ? (0.9f / peak) : 1.0f;

    // --- Write 16-bit mono WAV --------------------------------------------
    std::vector<uint8_t> data;
    data.reserve(total * 2);
    for (float v : buf) {
        int s = (int)std::lround(std::clamp(v * gain, -1.0f, 1.0f) * 32767.0f);
        putU16(data, (uint16_t)(int16_t)s);
    }

    std::vector<uint8_t> out;
    const uint32_t byteRate = (uint32_t)(sampleRate * 2);
    out.insert(out.end(), { 'R','I','F','F' });
    putU32(out, 36 + (uint32_t)data.size());
    out.insert(out.end(), { 'W','A','V','E', 'f','m','t',' ' });
    putU32(out, 16);            // fmt chunk size
    putU16(out, 1);             // PCM
    putU16(out, 1);             // mono
    putU32(out, (uint32_t)sampleRate);
    putU32(out, byteRate);
    putU16(out, 2);             // block align
    putU16(out, 16);            // bits per sample
    out.insert(out.end(), { 'd','a','t','a' });
    putU32(out, (uint32_t)data.size());
    out.insert(out.end(), data.begin(), data.end());

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(out.data()), (std::streamsize)out.size());
    return f.good();
}

} // namespace rhythm
