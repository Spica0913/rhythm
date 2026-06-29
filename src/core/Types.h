#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace rhythm {

// A pitch class is an integer 0..11 where 0 = C, 1 = C#/Db, ... 11 = B.
using PitchClass = int;

// A MIDI note number 0..127 (60 = middle C, C4 in the "C3=60" or "C4=60"
// convention depending on the host; we use 60 = C4 for display).
using MidiNote = int;

constexpr int kNotesPerOctave = 12;
constexpr MidiNote kMiddleC = 60; // C4

// Interval offsets in semitones from a chord root. Upper extensions use their
// "tall" values (an octave above the simple interval) so that voicings spread
// them naturally; reduce mod 12 when only the pitch class matters.
enum Interval {
    kRoot = 0,  kMin2 = 1,   kMaj2 = 2,   kMin3 = 3,  kMaj3 = 4,  kPerf4 = 5,
    kDim5 = 6,  kPerf5 = 7,  kAug5 = 8,   kMaj6 = 9,  kMin7 = 10, kMaj7 = 11,
    kFlat9 = 13, kNat9 = 14, kSharp9 = 15, kNat11 = 17, kSharp11 = 18,
    kFlat13 = 20, kNat13 = 21
};

// Names of the twelve pitch classes, preferring sharps.
inline const std::array<std::string, 12>& sharpNoteNames()
{
    static const std::array<std::string, 12> names = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    return names;
}

// Names of the twelve pitch classes, preferring flats.
inline const std::array<std::string, 12>& flatNoteNames()
{
    static const std::array<std::string, 12> names = {
        "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"
    };
    return names;
}

// Reduce any integer to the range 0..11 (handles negatives correctly).
inline PitchClass mod12(int value)
{
    int r = value % kNotesPerOctave;
    return r < 0 ? r + kNotesPerOctave : r;
}

// Pitch class of a MIDI note.
inline PitchClass pitchClassOf(MidiNote note) { return mod12(note); }

// Octave number of a MIDI note in the C4 = 60 convention.
inline int octaveOf(MidiNote note) { return note / kNotesPerOctave - 1; }

} // namespace rhythm
