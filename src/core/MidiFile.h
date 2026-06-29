#pragma once

#include "MidiEvent.h"

#include <string>

namespace rhythm {

// Minimal Standard MIDI File (format 0) writer. Converts a beat-based Sequence
// into a single-track .mid file at the given tempo. Returns false on I/O error.
// This lets the engine be used to export accompaniment without a DAW/JUCE.
bool writeMidiFile(const std::string& path,
                   const Sequence& sequence,
                   double bpm = 120.0,
                   int ticksPerQuarter = 480);

} // namespace rhythm
