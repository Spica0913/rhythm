#pragma once

#include "Chord.h"
#include "Types.h"

#include <vector>

namespace rhythm {

// Available voicing strategies.
enum class VoicingStyle {
    Close,      // root-position close stack (R 3 5 7 ...)
    Drop2,      // close voicing with 2nd-from-top dropped an octave
    Drop3,      // close voicing with 3rd-from-top dropped an octave
    Drop24,     // 2nd and 4th from top dropped an octave
    Shell,      // root, 3rd, 7th (or 6th) only
    RootlessA,  // 3 5 7 9 (no root) — "type A"
    RootlessB,  // 7 9 3 5 (no root) — "type B"
    Open,       // natural stack with extensions kept high
    PowerOctave // root + fifth + octave (rock/guitar)
};

const char* voicingStyleName(VoicingStyle style);

struct VoicingOptions {
    VoicingStyle style = VoicingStyle::Close;
    int centerNote = kMiddleC; // approximate register the voicing centres on
    bool includeBass = false;  // emit a separate low bass note for the root/slash
    int bassOctaveOffset = -2; // octaves below the voicing for the bass note
    bool voiceLeading = true;  // minimise movement from the previous voicing
    int maxNotes = 6;          // cap on the number of voiced notes (excl. bass)
};

// Produce concrete MIDI notes (sorted ascending) for a chord using the given
// options. If `previous` is supplied and voice leading is enabled, the voicing
// is octave-shifted to sit as close as possible to the previous one.
std::vector<MidiNote> voiceChord(const Chord& chord,
                                 const VoicingOptions& opts,
                                 const std::vector<MidiNote>* previous = nullptr);

} // namespace rhythm
