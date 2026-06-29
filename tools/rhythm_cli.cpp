// rhythm_cli — generate an accompaniment MIDI file from a chord progression
// using the rhythm core engine (no JUCE / DAW required).
#include "Generator.h"
#include "MidiFile.h"
#include "Progression.h"
#include "WavSynth.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>

using namespace rhythm;

namespace {

const std::map<std::string, VoicingStyle> kVoicings = {
    { "close", VoicingStyle::Close },     { "drop2", VoicingStyle::Drop2 },
    { "drop3", VoicingStyle::Drop3 },     { "drop24", VoicingStyle::Drop24 },
    { "shell", VoicingStyle::Shell },     { "rootlessa", VoicingStyle::RootlessA },
    { "rootlessb", VoicingStyle::RootlessB }, { "open", VoicingStyle::Open },
    { "power", VoicingStyle::PowerOctave }
};

const std::map<std::string, PatternId> kPatterns = {
    { "blockwhole", PatternId::BlockWhole }, { "blockbeats", PatternId::BlockBeats },
    { "offbeats", PatternId::OffBeats },     { "arpup", PatternId::ArpUp },
    { "arpdown", PatternId::ArpDown },       { "arpupdown", PatternId::ArpUpDown },
    { "alberti", PatternId::Alberti },       { "ballad", PatternId::Ballad },
    { "popcomp", PatternId::PopComp },       { "bossa", PatternId::Bossa },
    { "waltz", PatternId::Waltz },           { "offbeat8", PatternId::Offbeat8 }
};

void printOptions()
{
    std::printf("Voicings: ");
    for (auto& v : kVoicings) std::printf("%s ", v.first.c_str());
    std::printf("\nPatterns: ");
    for (auto& p : kPatterns) std::printf("%s ", p.first.c_str());
    std::printf("\n");
}

std::string lower(std::string s)
{
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

const char* argValue(int argc, char** argv, const char* flag, const char* def)
{
    for (int i = 1; i < argc - 1; ++i)
        if (std::strcmp(argv[i], flag) == 0) return argv[i + 1];
    return def;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc >= 2 && (std::strcmp(argv[1], "--help") == 0 ||
                      std::strcmp(argv[1], "-h") == 0)) {
        std::printf(
            "Usage: rhythm_cli [options]\n"
            "  --prog \"Cmaj7 | Am7 | Dm7 G7\"   chord progression\n"
            "  --voicing close|drop2|shell|...   voicing style\n"
            "  --pattern bossa|arpup|popcomp|... accompaniment pattern\n"
            "  --bpm 120                         tempo\n"
            "  --octave 0                        register offset (-2..3)\n"
            "  --bass                            add a low bass note\n"
            "  --no-voiceleading                 disable voice leading\n"
            "  --swing 0.0                       swing amount (0..1)\n"
            "  --out rhythm.mid                  output MIDI file\n"
            "  --wav rhythm.wav                  also render an audio preview\n"
            "  --list                            list voicings/patterns\n");
        return 0;
    }
    if (argc >= 2 && std::strcmp(argv[1], "--list") == 0) {
        printOptions();
        return 0;
    }

    std::string prog   = argValue(argc, argv, "--prog", "Cmaj7 | Am7 | Dm7 | G7");
    std::string voice  = lower(argValue(argc, argv, "--voicing", "drop2"));
    std::string pat    = lower(argValue(argc, argv, "--pattern", "popcomp"));
    double bpm         = std::atof(argValue(argc, argv, "--bpm", "120"));
    int octave         = std::atoi(argValue(argc, argv, "--octave", "0"));
    double swing       = std::atof(argValue(argc, argv, "--swing", "0"));
    std::string out    = argValue(argc, argv, "--out", "rhythm.mid");
    const char* wavArg = argValue(argc, argv, "--wav", "");

    bool bass = false, voiceLeading = true;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--bass") == 0) bass = true;
        if (std::strcmp(argv[i], "--no-voiceleading") == 0) voiceLeading = false;
    }

    GeneratorSettings s;
    auto vit = kVoicings.find(voice);
    if (vit == kVoicings.end()) { std::printf("Unknown voicing '%s'\n", voice.c_str()); printOptions(); return 1; }
    auto pit = kPatterns.find(pat);
    if (pit == kPatterns.end()) { std::printf("Unknown pattern '%s'\n", pat.c_str()); printOptions(); return 1; }

    s.voicing.style = vit->second;
    s.voicing.centerNote = kMiddleC + 12 * octave;
    s.voicing.includeBass = bass;
    s.voicing.voiceLeading = voiceLeading;
    s.pattern = pit->second;
    s.swing = swing;

    auto progression = parseProgression(prog);
    int validCount = 0;
    for (const auto& c : progression.chords) if (c.valid) ++validCount;
    if (validCount == 0) {
        std::printf("No valid chords parsed from: \"%s\"\n", prog.c_str());
        return 1;
    }

    auto seq = generate(progression, s);
    if (!writeMidiFile(out, seq, bpm)) {
        std::printf("Failed to write '%s'\n", out.c_str());
        return 1;
    }

    std::string wav = wavArg;
    if (!wav.empty()) {
        if (!writeWavPreview(wav, seq, bpm))
            std::printf("Warning: failed to write WAV '%s'\n", wav.c_str());
        else
            std::printf("Audio preview -> %s\n", wav.c_str());
    }

    std::printf("Generated %zu notes over %.0f beats -> %s\n",
                seq.size(), progression.totalBeats(), out.c_str());
    std::printf("  progression: %s\n", prog.c_str());
    std::printf("  voicing: %s   pattern: %s   bpm: %.0f\n",
                voice.c_str(), pat.c_str(), bpm);
    return 0;
}
