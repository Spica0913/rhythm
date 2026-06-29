#include "Generator.h"

#include <algorithm>
#include <cmath>

namespace rhythm {

void applySwing(Sequence& seq, double amount, double subdivision)
{
    if (amount <= 0.0) return;
    amount = std::clamp(amount, 0.0, 1.0);
    const double delay = subdivision * 0.5 * amount; // up to half a subdivision

    for (auto& e : seq) {
        // Is this note on an off-beat subdivision (e.g. the "and")?
        double pos = e.startBeats / subdivision;
        double frac = pos - std::floor(pos + 1e-9);
        bool onSubdivision = std::abs(frac) < 1e-6;
        long idx = std::lround(pos);
        if (onSubdivision && (idx % 2 != 0)) {
            e.startBeats += delay;
            e.lengthBeats = std::max(0.05, e.lengthBeats - delay);
        }
    }
}

Sequence generateChord(const Chord& chord,
                       double startBeats,
                       double durationBeats,
                       const GeneratorSettings& settings,
                       std::vector<MidiNote>* previousVoicing)
{
    Sequence seq;
    if (chord.isEmpty() || durationBeats <= 0.0) return seq;

    auto voicing = voiceChord(chord, settings.voicing,
                              (previousVoicing && !previousVoicing->empty())
                                  ? previousVoicing : nullptr);
    if (previousVoicing) *previousVoicing = voicing;
    if (voicing.empty()) return seq;

    Pattern pat = makePattern(settings.pattern);
    renderPattern(pat, voicing, startBeats, durationBeats,
                  settings.velocityScale, seq);

    for (auto& e : seq) {
        e.note = std::clamp(e.note + settings.transpose, 0, 127);
        e.channel = settings.midiChannel;
    }
    return seq;
}

Sequence generate(const Progression& progression,
                  const GeneratorSettings& settings)
{
    Sequence seq;
    std::vector<MidiNote> prev;
    double cursor = 0.0;

    for (const auto& pc : progression.chords) {
        if (!pc.valid || pc.chord.isEmpty()) {
            cursor += pc.durationBeats; // rest
            continue;
        }
        auto part = generateChord(pc.chord, cursor, pc.durationBeats,
                                  settings, &prev);
        seq.insert(seq.end(), part.begin(), part.end());
        cursor += pc.durationBeats;
    }

    if (settings.swing > 0.0)
        applySwing(seq, settings.swing);

    std::sort(seq.begin(), seq.end(),
              [](const NoteEvent& a, const NoteEvent& b) {
                  if (a.startBeats != b.startBeats) return a.startBeats < b.startBeats;
                  return a.note < b.note;
              });
    return seq;
}

} // namespace rhythm
