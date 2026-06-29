#pragma once

#include "Chord.h"
#include "Types.h"

#include <string>
#include <vector>

namespace rhythm {

// One chord slot within a progression with its duration in beats.
struct ProgressionChord {
    Chord chord;
    double durationBeats = 4.0;
    std::string symbol;   // original token (for display)
    bool valid = true;    // false if the token could not be parsed
};

struct Progression {
    std::vector<ProgressionChord> chords;
    double beatsPerBar = 4.0;

    double totalBeats() const {
        double t = 0.0;
        for (const auto& c : chords) t += c.durationBeats;
        return t;
    }
};

// Parse a lead-sheet style progression. Bars are separated by '|'; chords
// within a bar split the bar evenly. Examples:
//   "Cmaj7 | Am7 | Dm7 G7 | Cmaj7"
//   "| C  G | Am F |"
// The token "%" repeats the previous chord. Unparseable tokens are kept as
// invalid slots (treated as rests by the generator).
Progression parseProgression(const std::string& text, double beatsPerBar = 4.0);

} // namespace rhythm
