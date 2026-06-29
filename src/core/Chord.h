#pragma once

#include "Types.h"

#include <optional>
#include <string>
#include <vector>

namespace rhythm {

// A chord is represented as a root pitch class plus the set of intervals
// (in semitones, measured from the root) that make up the chord, plus an
// optional bass note for slash chords (e.g. C/E).
//
// Intervals are stored as semitone offsets from the root and may exceed 12
// for upper extensions (e.g. a 9th is 14, an 11th is 17, a 13th is 21).
struct Chord {
    PitchClass root = 0;                 // 0..11
    std::vector<int> intervals;          // semitones from root, sorted ascending
    std::optional<PitchClass> bass;      // slash-chord bass note, if any
    std::string symbol;                  // original/normalised symbol, e.g. "Cmaj7"

    // The pitch classes contained in the chord (root + intervals), de-duplicated.
    std::vector<PitchClass> pitchClasses() const;

    // True if no intervals are defined (an "empty"/rest chord).
    bool isEmpty() const { return intervals.empty(); }
};

// Parse a chord symbol such as "Cmaj7", "Am7", "G7b9", "F#m7b5", "Bbsus4",
// "Dm7/G", "C/E". Returns std::nullopt if the symbol cannot be parsed.
//
// Supported roots: A-G with optional # / b (and unicode ♯ ♭).
// Supported qualities (case-insensitive where unambiguous):
//   maj triad (default / "maj" / "M"), m / min / -, dim / o, aug / +,
//   sus2, sus4, 5 (power chord),
//   6, m6, maj7 / M7 / Δ, 7, m7 / min7, m7b5 / ø, dim7 / o7, mMaj7,
//   9, maj9, m9, 11, maj11, m11, 13, maj13, m13, add9, 6/9,
//   alterations b5 #5 b9 #9 #11 b13 (in parentheses or bare).
std::optional<Chord> parseChord(const std::string& symbol);

// Identify the most likely chord from a set of sounding pitch classes (chord
// detection used for MIDI-input mode). `lowestNote` is the lowest sounding
// MIDI note, used to bias the root / detect slash bass. Returns nullopt if the
// set is too small (< 2 notes) to identify.
std::optional<Chord> detectChord(const std::vector<MidiNote>& notes);

// Human-readable chord name from a Chord (best-effort, used for display).
std::string chordToString(const Chord& chord, bool preferFlats = false);

} // namespace rhythm
