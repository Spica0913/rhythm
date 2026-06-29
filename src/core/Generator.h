#pragma once

#include "MidiEvent.h"
#include "Pattern.h"
#include "Progression.h"
#include "Voicing.h"

namespace rhythm {

// Top-level settings that drive sequence generation.
struct GeneratorSettings {
    VoicingOptions voicing;            // voicing style / register / voice leading
    PatternId pattern = PatternId::BlockBeats;
    float velocityScale = 1.0f;        // global velocity multiplier (0..~1.5)
    double swing = 0.0;                // 0 = straight, up to ~0.6 = heavy swing
    int humanizeTimingMs = 0;          // unused at beat level; kept for the host
    int transpose = 0;                 // semitones applied to all output notes
    int midiChannel = 1;               // 1..16
};

// Generate a MIDI sequence (in beats) from a chord progression. Applies the
// chosen voicing per chord (with voice leading between chords), renders the
// accompaniment/rhythm pattern, and applies swing/transpose.
Sequence generate(const Progression& progression,
                  const GeneratorSettings& settings);

// Generate a single chord's worth of events (used by real-time MIDI-input mode
// where the chord is detected live and held for `durationBeats`). `previous`
// carries voice-leading state across calls.
Sequence generateChord(const Chord& chord,
                       double startBeats,
                       double durationBeats,
                       const GeneratorSettings& settings,
                       std::vector<MidiNote>* previousVoicing);

// Apply a swing feel to a sequence in place: delays the off-beat eighth notes.
// `amount` 0..1 maps to 50%..~75% of the eighth-note slot.
void applySwing(Sequence& seq, double amount, double subdivision = 0.5);

} // namespace rhythm
