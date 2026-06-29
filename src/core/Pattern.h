#pragma once

#include "MidiEvent.h"
#include "Types.h"

#include <string>
#include <vector>

namespace rhythm {

// Which voice(s) of a voicing a pattern step should sound.
enum class VoiceSelector {
    All,       // whole chord (block)
    Bass,      // lowest note only
    Top,       // highest note only
    Upper,     // everything except the lowest note
    ArpUp,     // next note cycling upward through the voicing
    ArpDown,   // next note cycling downward through the voicing
    Index      // a specific index from the bottom (see PatternStep::index)
};

// One event within a one-bar pattern, positioned in beats from the bar start.
struct PatternStep {
    double startBeats = 0.0;
    double lengthBeats = 1.0;
    float velocity = 0.8f;        // 0..1
    VoiceSelector voices = VoiceSelector::All;
    int index = 0;               // used when voices == Index
};

// A reusable rhythm/accompaniment pattern spanning `barLengthBeats` beats
// (defaults to a 4/4 bar). Patterns are tiled across the duration of each
// chord by the generator.
struct Pattern {
    std::string name;
    double barLengthBeats = 4.0;
    std::vector<PatternStep> steps;
};

// Built-in accompaniment patterns.
enum class PatternId {
    BlockWhole,    // one sustained chord per bar
    BlockBeats,    // chord stab on every beat
    OffBeats,      // chord on the off-beats (the "&"s)
    ArpUp,         // ascending arpeggio, eighth notes
    ArpDown,       // descending arpeggio, eighth notes
    ArpUpDown,     // up then down
    Alberti,       // low-high-mid-high (classical)
    Ballad,        // bass on 1/3, chord on 2/4
    PopComp,       // common pop/rock comping
    Bossa,         // bossa-nova style comping
    Waltz,         // 3/4 oom-pah-pah
    Offbeat8       // syncopated eighth-note guitar/keys
};

const char* patternName(PatternId id);

// Build one of the built-in patterns.
Pattern makePattern(PatternId id);

// Realise a pattern over a chord voicing, producing note events whose times
// are offset by `startBeats` and tiled to fill `durationBeats`. The pattern's
// own bar length determines the tile size.
void renderPattern(const Pattern& pattern,
                   const std::vector<MidiNote>& voicing,
                   double startBeats,
                   double durationBeats,
                   float velocityScale,
                   Sequence& out);

} // namespace rhythm
