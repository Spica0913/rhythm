#include "Voicing.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <optional>

namespace rhythm {

const char* voicingStyleName(VoicingStyle style)
{
    switch (style) {
        case VoicingStyle::Close:       return "Close";
        case VoicingStyle::Drop2:       return "Drop 2";
        case VoicingStyle::Drop3:       return "Drop 3";
        case VoicingStyle::Drop24:      return "Drop 2+4";
        case VoicingStyle::Shell:       return "Shell";
        case VoicingStyle::RootlessA:   return "Rootless A";
        case VoicingStyle::RootlessB:   return "Rootless B";
        case VoicingStyle::Open:        return "Open";
        case VoicingStyle::PowerOctave: return "Power/Octave";
    }
    return "Close";
}

namespace {

// Place a pitch class in the octave nearest to (and at or above) `floor`.
MidiNote noteAtOrAbove(PitchClass pc, MidiNote floorNote)
{
    int n = floorNote + mod12(pc - floorNote);
    return n;
}

// Build the close (root-position) stack of the chord's tones starting near the
// centre register. Returns the chord-member intervals in stacking order along
// with the resulting MIDI notes.
std::vector<MidiNote> closeStack(const Chord& chord, int centerNote)
{
    // Root placed in the octave whose root is nearest the centre.
    int rootNote = centerNote - 4; // bias root slightly below centre
    rootNote = rootNote - mod12(rootNote - chord.root); // snap down to root pc

    std::vector<MidiNote> notes;
    notes.push_back(rootNote);

    MidiNote prev = rootNote;
    for (int iv : chord.intervals) {
        PitchClass pc = mod12(chord.root + iv);
        MidiNote n = noteAtOrAbove(pc, prev + 1);
        notes.push_back(n);
        prev = n;
    }
    return notes;
}

// Find a chord member realised as a MIDI note, given a list of acceptable
// intervals (first match wins). Searches near `nearNote`.
std::optional<MidiNote> findMember(const Chord& chord,
                                   std::initializer_list<int> acceptable,
                                   MidiNote nearNote)
{
    for (int want : acceptable) {
        for (int iv : chord.intervals) {
            if (mod12(iv) == mod12(want)) {
                PitchClass pc = mod12(chord.root + iv);
                // Nearest octave to nearNote.
                int base = nearNote - mod12(nearNote - pc);
                if (std::abs(base + 12 - nearNote) < std::abs(base - nearNote))
                    base += 12;
                return base;
            }
        }
    }
    return std::nullopt;
}

void dropTopN(std::vector<MidiNote>& notes, int fromTop)
{
    // fromTop is 1-based index counting from the highest note.
    if ((int)notes.size() < fromTop) return;
    std::sort(notes.begin(), notes.end());
    int idx = (int)notes.size() - fromTop;
    notes[idx] -= 12;
    std::sort(notes.begin(), notes.end());
}

double centroid(const std::vector<MidiNote>& notes)
{
    if (notes.empty()) return 0.0;
    return std::accumulate(notes.begin(), notes.end(), 0.0) / notes.size();
}

} // namespace

std::vector<MidiNote> voiceChord(const Chord& chord,
                                 const VoicingOptions& opts,
                                 const std::vector<MidiNote>* previous)
{
    if (chord.isEmpty()) return {};

    std::vector<MidiNote> notes;
    const int c = opts.centerNote;

    switch (opts.style) {
        case VoicingStyle::Close:
        case VoicingStyle::Open: {
            notes = closeStack(chord, c);
            if (opts.style == VoicingStyle::Open && notes.size() >= 4) {
                // Spread: drop the second voice an octave for an open sound.
                dropTopN(notes, 2);
            }
            break;
        }
        case VoicingStyle::Drop2: {
            notes = closeStack(chord, c);
            dropTopN(notes, 2);
            break;
        }
        case VoicingStyle::Drop3: {
            notes = closeStack(chord, c);
            dropTopN(notes, 3);
            break;
        }
        case VoicingStyle::Drop24: {
            notes = closeStack(chord, c);
            dropTopN(notes, 2);
            dropTopN(notes, 4);
            break;
        }
        case VoicingStyle::Shell: {
            MidiNote root = c - mod12(c - chord.root);
            notes.push_back(root);
            if (auto third = findMember(chord, { kMin3, kMaj3, kPerf4, kMaj2 }, root + 4))
                notes.push_back(*third);
            if (auto sev = findMember(chord, { kMin7, kMaj7, kMaj6 }, root + 10))
                notes.push_back(*sev);
            break;
        }
        case VoicingStyle::RootlessA: {
            MidiNote base = c;
            if (auto m = findMember(chord, { kMin3, kMaj3 }, base)) notes.push_back(*m);
            if (auto m = findMember(chord, { kPerf5, kDim5, kAug5 }, base + 4)) notes.push_back(*m);
            if (auto m = findMember(chord, { kMin7, kMaj7, kMaj6 }, base + 7)) notes.push_back(*m);
            if (auto m = findMember(chord, { kNat9, kFlat9, kSharp9 }, base + 11)) notes.push_back(*m);
            if (notes.size() < 2) notes = closeStack(chord, c); // fallback
            break;
        }
        case VoicingStyle::RootlessB: {
            MidiNote base = c;
            if (auto m = findMember(chord, { kMin7, kMaj7, kMaj6 }, base)) notes.push_back(*m);
            if (auto m = findMember(chord, { kNat9, kFlat9, kSharp9 }, base + 3)) notes.push_back(*m);
            if (auto m = findMember(chord, { kMin3, kMaj3 }, base + 7)) notes.push_back(*m);
            if (auto m = findMember(chord, { kPerf5, kDim5, kAug5 }, base + 10)) notes.push_back(*m);
            if (notes.size() < 2) notes = closeStack(chord, c); // fallback
            break;
        }
        case VoicingStyle::PowerOctave: {
            MidiNote root = c - mod12(c - chord.root);
            notes.push_back(root);
            notes.push_back(root + 7);
            notes.push_back(root + 12);
            break;
        }
    }

    std::sort(notes.begin(), notes.end());

    // Cap voicing size by trimming inner notes (keep outer voices).
    if ((int)notes.size() > opts.maxNotes && opts.maxNotes >= 2) {
        std::vector<MidiNote> trimmed;
        trimmed.push_back(notes.front());
        int innerWanted = opts.maxNotes - 2;
        int innerAvail = (int)notes.size() - 2;
        for (int i = 1; i <= innerAvail && (int)trimmed.size() < opts.maxNotes - 1; ++i) {
            // sample inner notes evenly
            int idx = 1 + (i * innerAvail) / (innerWanted + 1);
            idx = std::min(idx, (int)notes.size() - 2);
            if (trimmed.empty() || notes[idx] != trimmed.back())
                trimmed.push_back(notes[idx]);
        }
        trimmed.push_back(notes.back());
        std::sort(trimmed.begin(), trimmed.end());
        trimmed.erase(std::unique(trimmed.begin(), trimmed.end()), trimmed.end());
        notes = trimmed;
    }

    // Voice leading: octave-shift the whole voicing toward the previous one.
    if (opts.voiceLeading && previous && !previous->empty() && !notes.empty()) {
        double target = centroid(*previous);
        double cur = centroid(notes);
        int shift = (int)std::lround((target - cur) / 12.0);
        if (shift != 0)
            for (auto& n : notes) n += shift * 12;
    }

    // Keep within a sane MIDI register.
    while (!notes.empty() && centroid(notes) > 96) for (auto& n : notes) n -= 12;
    while (!notes.empty() && centroid(notes) < 36) for (auto& n : notes) n += 12;

    // Optional separate bass note (root or slash bass).
    if (opts.includeBass && !notes.empty()) {
        PitchClass bassPc = chord.bass ? *chord.bass : chord.root;
        MidiNote bass = notes.front() + opts.bassOctaveOffset * 12;
        bass = bass - mod12(bass - bassPc); // snap to bass pitch class at/under
        if (bass >= 12) {
            notes.insert(notes.begin(), bass);
        }
    } else if (chord.bass) {
        // Slash chord without separate bass: ensure the bass pc is the lowest.
        MidiNote bass = noteAtOrAbove(*chord.bass, notes.front() - 12);
        if (bass < notes.front())
            notes.insert(notes.begin(), bass);
    }

    // Clamp to valid MIDI range.
    for (auto& n : notes) n = std::clamp(n, 0, 127);
    std::sort(notes.begin(), notes.end());
    notes.erase(std::unique(notes.begin(), notes.end()), notes.end());
    return notes;
}

} // namespace rhythm
