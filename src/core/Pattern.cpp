#include "Pattern.h"

#include <algorithm>
#include <cmath>

namespace rhythm {

const char* patternName(PatternId id)
{
    switch (id) {
        case PatternId::BlockWhole: return "Block (whole)";
        case PatternId::BlockBeats: return "Block (beats)";
        case PatternId::OffBeats:   return "Off-beats";
        case PatternId::ArpUp:      return "Arp up";
        case PatternId::ArpDown:    return "Arp down";
        case PatternId::ArpUpDown:  return "Arp up/down";
        case PatternId::Alberti:    return "Alberti bass";
        case PatternId::Ballad:     return "Ballad";
        case PatternId::PopComp:    return "Pop comp";
        case PatternId::Bossa:      return "Bossa nova";
        case PatternId::Waltz:      return "Waltz";
        case PatternId::Offbeat8:   return "Off-beat 8ths";
    }
    return "Block";
}

namespace {

PatternStep step(double start, double len, float vel,
                 VoiceSelector v, int idx = 0)
{
    PatternStep s;
    s.startBeats = start;
    s.lengthBeats = len;
    s.velocity = vel;
    s.voices = v;
    s.index = idx;
    return s;
}

} // namespace

Pattern makePattern(PatternId id)
{
    Pattern p;
    p.name = patternName(id);
    p.barLengthBeats = 4.0;

    switch (id) {
        case PatternId::BlockWhole:
            p.steps = { step(0.0, 4.0, 0.85f, VoiceSelector::All) };
            break;

        case PatternId::BlockBeats:
            for (int b = 0; b < 4; ++b)
                p.steps.push_back(step(b, 0.9, b == 0 ? 0.9f : 0.75f,
                                       VoiceSelector::All));
            break;

        case PatternId::OffBeats:
            // Bass on 1, chords on every off-beat (the "and" of each beat).
            p.steps.push_back(step(0.0, 1.0, 0.85f, VoiceSelector::Bass));
            for (int b = 0; b < 4; ++b)
                p.steps.push_back(step(b + 0.5, 0.4, 0.7f, VoiceSelector::Upper));
            break;

        case PatternId::ArpUp:
            for (int i = 0; i < 8; ++i)
                p.steps.push_back(step(i * 0.5, 0.5, i % 2 ? 0.65f : 0.78f,
                                       VoiceSelector::ArpUp));
            break;

        case PatternId::ArpDown:
            for (int i = 0; i < 8; ++i)
                p.steps.push_back(step(i * 0.5, 0.5, i % 2 ? 0.65f : 0.78f,
                                       VoiceSelector::ArpDown));
            break;

        case PatternId::ArpUpDown:
            for (int i = 0; i < 4; ++i)
                p.steps.push_back(step(i * 0.5, 0.5, 0.75f, VoiceSelector::ArpUp));
            for (int i = 0; i < 4; ++i)
                p.steps.push_back(step(2.0 + i * 0.5, 0.5, 0.7f,
                                       VoiceSelector::ArpDown));
            break;

        case PatternId::Alberti:
            // Classic low - high - middle - high, eighth notes.
            for (int b = 0; b < 4; ++b) {
                double t = b;
                p.steps.push_back(step(t,        0.5, 0.78f, VoiceSelector::Index, 0));
                p.steps.push_back(step(t + 0.5,  0.5, 0.7f,  VoiceSelector::Top));
            }
            break;

        case PatternId::Ballad:
            // Bass on beats 1 & 3, chord on 2 & 4.
            p.steps.push_back(step(0.0, 1.0, 0.85f, VoiceSelector::Bass));
            p.steps.push_back(step(1.0, 1.0, 0.7f,  VoiceSelector::Upper));
            p.steps.push_back(step(2.0, 1.0, 0.8f,  VoiceSelector::Bass));
            p.steps.push_back(step(3.0, 1.0, 0.7f,  VoiceSelector::Upper));
            break;

        case PatternId::PopComp:
            // Bass on 1, chord stabs with a syncopated push into beat 3.
            p.steps.push_back(step(0.0, 1.0, 0.85f, VoiceSelector::Bass));
            p.steps.push_back(step(1.0, 0.5, 0.72f, VoiceSelector::Upper));
            p.steps.push_back(step(2.0, 1.0, 0.82f, VoiceSelector::All));
            p.steps.push_back(step(2.5, 0.5, 0.68f, VoiceSelector::Upper));
            p.steps.push_back(step(3.0, 0.5, 0.72f, VoiceSelector::Upper));
            p.steps.push_back(step(3.5, 0.5, 0.7f,  VoiceSelector::Upper));
            break;

        case PatternId::Bossa:
            // Bossa-nova comp: bass 1 & 3, chord on the classic 3-3-2 push.
            p.steps.push_back(step(0.0, 1.0, 0.8f,  VoiceSelector::Bass));
            p.steps.push_back(step(1.5, 0.5, 0.72f, VoiceSelector::Upper));
            p.steps.push_back(step(2.0, 1.0, 0.8f,  VoiceSelector::Bass));
            p.steps.push_back(step(2.5, 0.5, 0.7f,  VoiceSelector::Upper));
            p.steps.push_back(step(3.5, 0.5, 0.72f, VoiceSelector::Upper));
            break;

        case PatternId::Waltz:
            p.barLengthBeats = 3.0;
            p.steps.push_back(step(0.0, 1.0, 0.85f, VoiceSelector::Bass));
            p.steps.push_back(step(1.0, 0.9, 0.7f,  VoiceSelector::Upper));
            p.steps.push_back(step(2.0, 0.9, 0.7f,  VoiceSelector::Upper));
            break;

        case PatternId::Offbeat8:
            // Reggae/ska-ish: chord only on every off-beat.
            for (int b = 0; b < 4; ++b)
                p.steps.push_back(step(b + 0.5, 0.4, 0.75f, VoiceSelector::All));
            break;
    }
    return p;
}

namespace {

// Resolve which notes a step plays from the voicing. `arpCounter` advances for
// arpeggio selectors so successive Arp steps walk through the voicing.
std::vector<MidiNote> selectNotes(const PatternStep& s,
                                  const std::vector<MidiNote>& voicing,
                                  int& arpCounter)
{
    if (voicing.empty()) return {};
    const int n = (int)voicing.size();

    switch (s.voices) {
        case VoiceSelector::All:
            return voicing;
        case VoiceSelector::Bass:
            return { voicing.front() };
        case VoiceSelector::Top:
            return { voicing.back() };
        case VoiceSelector::Upper:
            return (n > 1) ? std::vector<MidiNote>(voicing.begin() + 1, voicing.end())
                           : voicing;
        case VoiceSelector::Index: {
            int i = std::clamp(s.index, 0, n - 1);
            return { voicing[i] };
        }
        case VoiceSelector::ArpUp: {
            int i = arpCounter % n;
            ++arpCounter;
            return { voicing[i] };
        }
        case VoiceSelector::ArpDown: {
            int i = (n - 1) - (arpCounter % n);
            ++arpCounter;
            return { voicing[i] };
        }
    }
    return voicing;
}

} // namespace

void renderPattern(const Pattern& pattern,
                   const std::vector<MidiNote>& voicing,
                   double startBeats,
                   double durationBeats,
                   float velocityScale,
                   Sequence& out)
{
    if (voicing.empty() || pattern.steps.empty() || durationBeats <= 0.0)
        return;

    const double bar = pattern.barLengthBeats > 0 ? pattern.barLengthBeats : 4.0;
    int arpCounter = 0;

    // Tile the pattern across the chord's duration.
    for (double tile = 0.0; tile < durationBeats - 1e-6; tile += bar) {
        for (const auto& s : pattern.steps) {
            double localStart = tile + s.startBeats;
            if (localStart >= durationBeats - 1e-6) continue;

            // Clip note length so it never runs past the chord's slot.
            double len = std::min(s.lengthBeats, durationBeats - localStart);
            if (len <= 0.0) continue;

            auto notes = selectNotes(s, voicing, arpCounter);
            for (MidiNote m : notes) {
                NoteEvent e;
                e.startBeats = startBeats + localStart;
                e.lengthBeats = len;
                e.note = m;
                e.velocity = std::clamp(s.velocity * velocityScale, 0.0f, 1.0f);
                out.push_back(e);
            }
        }
    }
}

} // namespace rhythm
