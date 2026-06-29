#pragma once

#include "MidiEvent.h"

#include <string>

namespace rhythm {

// Render a generated Sequence to a 16-bit mono WAV file using a small built-in
// synth (decaying harmonic tone). This is for *previewing* the engine's output
// as audio without a DAW — not the final instrument sound. Returns false on I/O
// error.
bool writeWavPreview(const std::string& path,
                     const Sequence& sequence,
                     double bpm = 120.0,
                     int sampleRate = 44100);

} // namespace rhythm
