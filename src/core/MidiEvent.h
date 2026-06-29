#pragma once

#include "Types.h"

#include <vector>

namespace rhythm {

// A note event positioned on a timeline measured in quarter-note beats.
// startBeats/lengthBeats are floating-point beats from the start of the
// generated sequence. The plugin converts beats to samples using host tempo.
struct NoteEvent {
    double startBeats = 0.0;
    double lengthBeats = 0.0;
    MidiNote note = 60;
    float velocity = 0.8f; // 0..1
    int channel = 1;       // 1..16

    double endBeats() const { return startBeats + lengthBeats; }
};

// A whole generated sequence is just a time-ordered list of note events.
using Sequence = std::vector<NoteEvent>;

} // namespace rhythm
