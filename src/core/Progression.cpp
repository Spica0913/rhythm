#include "Progression.h"

#include <sstream>

namespace rhythm {

namespace {

std::vector<std::string> splitTokens(const std::string& bar)
{
    std::vector<std::string> out;
    std::istringstream iss(bar);
    std::string tok;
    while (iss >> tok) out.push_back(tok);
    return out;
}

std::vector<std::string> splitBars(const std::string& text)
{
    std::vector<std::string> bars;
    std::string cur;
    for (char c : text) {
        if (c == '|') {
            bars.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    bars.push_back(cur);
    return bars;
}

} // namespace

Progression parseProgression(const std::string& text, double beatsPerBar)
{
    Progression prog;
    prog.beatsPerBar = beatsPerBar;

    std::optional<Chord> previous;

    for (const auto& bar : splitBars(text)) {
        auto tokens = splitTokens(bar);
        if (tokens.empty()) continue; // skip empty bars (e.g. leading '|')

        double slot = beatsPerBar / (double)tokens.size();
        for (const auto& tok : tokens) {
            ProgressionChord pc;
            pc.symbol = tok;
            pc.durationBeats = slot;

            if (tok == "%" || tok == "/" ) {
                if (previous) { pc.chord = *previous; pc.valid = true; }
                else { pc.valid = false; }
            } else if (auto parsed = parseChord(tok)) {
                pc.chord = *parsed;
                pc.valid = true;
                previous = parsed;
            } else {
                pc.valid = false;
            }
            prog.chords.push_back(pc);
        }
    }

    return prog;
}

} // namespace rhythm
