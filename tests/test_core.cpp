// Lightweight, dependency-free unit tests for the rhythm core engine.
#include "Chord.h"
#include "Generator.h"
#include "Pattern.h"
#include "Progression.h"
#include "Voicing.h"

#include <cstdio>
#include <set>
#include <string>
#include <vector>

using namespace rhythm;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        ++g_checks;                                                        \
        if (!(cond)) {                                                     \
            ++g_failures;                                                  \
            std::printf("  FAIL: %s  (line %d)\n", #cond, __LINE__);       \
        }                                                                  \
    } while (0)

static std::set<int> pcSet(const Chord& c)
{
    auto v = c.pitchClasses();
    return { v.begin(), v.end() };
}

static bool sameSet(const std::set<int>& a, std::initializer_list<int> b)
{
    return a == std::set<int>(b);
}

static void testNoteHelpers()
{
    std::printf("[note helpers]\n");
    CHECK(mod12(0) == 0);
    CHECK(mod12(12) == 0);
    CHECK(mod12(-1) == 11);
    CHECK(mod12(13) == 1);
    CHECK(pitchClassOf(60) == 0); // C
    CHECK(pitchClassOf(69) == 9); // A
}

static void testChordParsing()
{
    std::printf("[chord parsing]\n");

    auto c = parseChord("C");
    CHECK(c.has_value());
    CHECK(c->root == 0);
    CHECK(sameSet(pcSet(*c), {0, 4, 7}));

    auto cm = parseChord("Cm");
    CHECK(cm && sameSet(pcSet(*cm), {0, 3, 7}));

    auto cmaj7 = parseChord("Cmaj7");
    CHECK(cmaj7 && sameSet(pcSet(*cmaj7), {0, 4, 7, 11}));

    auto c7 = parseChord("C7");
    CHECK(c7 && sameSet(pcSet(*c7), {0, 4, 7, 10}));

    auto cm7 = parseChord("Cm7");
    CHECK(cm7 && sameSet(pcSet(*cm7), {0, 3, 7, 10}));

    auto half = parseChord("Cm7b5");
    CHECK(half && sameSet(pcSet(*half), {0, 3, 6, 10}));

    auto dim7 = parseChord("Cdim7");
    CHECK(dim7 && sameSet(pcSet(*dim7), {0, 3, 6, 9}));

    auto aug = parseChord("Caug");
    CHECK(aug && sameSet(pcSet(*aug), {0, 4, 8}));

    auto sus4 = parseChord("Csus4");
    CHECK(sus4 && sameSet(pcSet(*sus4), {0, 5, 7}));

    auto sus2 = parseChord("Csus2");
    CHECK(sus2 && sameSet(pcSet(*sus2), {0, 2, 7}));

    auto six = parseChord("C6");
    CHECK(six && sameSet(pcSet(*six), {0, 4, 7, 9}));

    auto m6 = parseChord("Cm6");
    CHECK(m6 && sameSet(pcSet(*m6), {0, 3, 7, 9}));

    auto c9 = parseChord("C9");
    CHECK(c9 && sameSet(pcSet(*c9), {0, 4, 7, 10, 2})); // 9th = D = pc 2

    auto cmaj9 = parseChord("Cmaj9");
    CHECK(cmaj9 && sameSet(pcSet(*cmaj9), {0, 4, 7, 11, 2}));

    auto c13 = parseChord("C13");
    CHECK(c13 && sameSet(pcSet(*c13), {0, 4, 7, 10, 2, 9})); // R3 5 b7 9 13

    auto alt = parseChord("C7b9");
    CHECK(alt && sameSet(pcSet(*alt), {0, 4, 7, 10, 1})); // b9 = Db = pc1

    auto sharp = parseChord("F#m7");
    CHECK(sharp && sharp->root == 6 && sameSet(pcSet(*sharp), {6, 9, 1, 4}));

    auto flat = parseChord("Bbmaj7");
    CHECK(flat && flat->root == 10);

    auto slash = parseChord("C/E");
    CHECK(slash && slash->root == 0 && slash->bass && *slash->bass == 4);

    auto minDash = parseChord("D-7");
    CHECK(minDash && minDash->root == 2 && sameSet(pcSet(*minDash), {2, 5, 9, 0}));

    // Garbage should fail.
    CHECK(!parseChord("H").has_value());
    CHECK(!parseChord("xyz").has_value());
    CHECK(!parseChord("").has_value());
}

static void testChordDetection()
{
    std::printf("[chord detection]\n");

    // C major triad.
    auto c = detectChord({60, 64, 67});
    CHECK(c.has_value());
    CHECK(c && c->root == 0);
    CHECK(c && sameSet(pcSet(*c), {0, 4, 7}));

    // A minor 7 (A C E G).
    auto am7 = detectChord({57, 60, 64, 67});
    CHECK(am7 && am7->root == 9);
    CHECK(am7 && sameSet(pcSet(*am7), {9, 0, 4, 7}));

    // G7 (G B D F).
    auto g7 = detectChord({55, 59, 62, 65});
    CHECK(g7 && g7->root == 7);
    CHECK(g7 && sameSet(pcSet(*g7), {7, 11, 2, 5}));

    // Too few notes -> nothing.
    CHECK(!detectChord({60}).has_value());
}

static void testVoicing()
{
    std::printf("[voicing]\n");

    auto cmaj7 = parseChord("Cmaj7");
    CHECK(cmaj7.has_value());

    VoicingOptions close;
    close.style = VoicingStyle::Close;
    close.voiceLeading = false;
    auto v = voiceChord(*cmaj7, close);
    CHECK(v.size() == 4);
    // Ascending and all are chord tones.
    for (size_t i = 1; i < v.size(); ++i) CHECK(v[i] > v[i - 1]);
    std::set<int> wanted = {0, 4, 7, 11};
    for (auto n : v) CHECK(wanted.count(pitchClassOf(n)) == 1);

    // Drop2 spreads the voicing wider than close.
    VoicingOptions drop = close;
    drop.style = VoicingStyle::Drop2;
    auto vd = voiceChord(*cmaj7, drop);
    CHECK(vd.size() == 4);
    int closeSpan = v.back() - v.front();
    int dropSpan = vd.back() - vd.front();
    CHECK(dropSpan > closeSpan);

    // Shell = 3 notes (R, 3, 7).
    VoicingOptions shell = close;
    shell.style = VoicingStyle::Shell;
    auto vs = voiceChord(*cmaj7, shell);
    CHECK(vs.size() == 3);
    std::set<int> shellPcs;
    for (auto n : vs) shellPcs.insert(pitchClassOf(n));
    CHECK(sameSet(shellPcs, {0, 4, 11}));

    // Voice leading: G7 after Cmaj7 should land close to it.
    auto g7 = parseChord("G7");
    VoicingOptions vl = close;
    vl.voiceLeading = true;
    auto vc = voiceChord(*cmaj7, vl);
    auto vg = voiceChord(*g7, vl, &vc);
    double avgC = 0, avgG = 0;
    for (auto n : vc) avgC += n;
    for (auto n : vg) avgG += n;
    avgC /= vc.size();
    avgG /= vg.size();
    CHECK(std::abs(avgC - avgG) < 7.0); // within ~a fifth

    // Power chord.
    VoicingOptions po = close;
    po.style = VoicingStyle::PowerOctave;
    auto vp = voiceChord(*parseChord("C"), po);
    CHECK(vp.size() == 3);
    CHECK(pitchClassOf(vp[0]) == 0 && pitchClassOf(vp[1]) == 7);

    // Bass note option adds a low root.
    VoicingOptions wb = close;
    wb.includeBass = true;
    auto vb = voiceChord(*cmaj7, wb);
    CHECK(vb.size() == 5);
    CHECK(vb.front() < v.front());
    CHECK(pitchClassOf(vb.front()) == 0);
}

static void testPatterns()
{
    std::printf("[patterns]\n");

    std::vector<MidiNote> voicing = {60, 64, 67, 71};

    // Block whole: one event per chord tone for the whole bar.
    {
        Sequence seq;
        renderPattern(makePattern(PatternId::BlockWhole), voicing, 0.0, 4.0, 1.0f, seq);
        CHECK(seq.size() == 4);
        for (auto& e : seq) CHECK(std::abs(e.lengthBeats - 4.0) < 1e-6);
    }

    // Block beats: 4 beats x 4 notes = 16 events.
    {
        Sequence seq;
        renderPattern(makePattern(PatternId::BlockBeats), voicing, 0.0, 4.0, 1.0f, seq);
        CHECK(seq.size() == 16);
    }

    // Arp up: 8 single-note events walking upward then wrapping.
    {
        Sequence seq;
        renderPattern(makePattern(PatternId::ArpUp), voicing, 0.0, 4.0, 1.0f, seq);
        CHECK(seq.size() == 8);
        CHECK(seq[0].note == 60);
        CHECK(seq[1].note == 64);
        CHECK(seq[2].note == 67);
        CHECK(seq[3].note == 71);
        CHECK(seq[4].note == 60); // wrapped
    }

    // Tiling: an 8-event 1-bar pattern over 2 bars = 16 events.
    {
        Sequence seq;
        renderPattern(makePattern(PatternId::ArpUp), voicing, 0.0, 8.0, 1.0f, seq);
        CHECK(seq.size() == 16);
    }

    // Waltz uses a 3-beat bar.
    {
        Pattern w = makePattern(PatternId::Waltz);
        CHECK(std::abs(w.barLengthBeats - 3.0) < 1e-6);
    }

    // Notes never extend past the chord slot.
    {
        Sequence seq;
        renderPattern(makePattern(PatternId::BlockWhole), voicing, 0.0, 2.0, 1.0f, seq);
        for (auto& e : seq) CHECK(e.endBeats() <= 2.0 + 1e-6);
    }
}

static void testProgression()
{
    std::printf("[progression]\n");

    auto prog = parseProgression("Cmaj7 | Am7 | Dm7 G7 | Cmaj7");
    CHECK(prog.chords.size() == 5);
    CHECK(std::abs(prog.chords[0].durationBeats - 4.0) < 1e-6);
    // The two chords in bar 3 split the bar.
    CHECK(std::abs(prog.chords[2].durationBeats - 2.0) < 1e-6);
    CHECK(std::abs(prog.chords[3].durationBeats - 2.0) < 1e-6);
    CHECK(std::abs(prog.totalBeats() - 16.0) < 1e-6);

    // Leading/trailing bar separators are ignored.
    auto p2 = parseProgression("| C | G |");
    CHECK(p2.chords.size() == 2);

    // "%" repeats the previous chord.
    auto p3 = parseProgression("C | %");
    CHECK(p3.chords.size() == 2);
    CHECK(p3.chords[1].valid);
    CHECK(p3.chords[1].chord.root == 0);

    // Invalid token becomes an invalid (rest) slot.
    auto p4 = parseProgression("C | zzz");
    CHECK(p4.chords.size() == 2);
    CHECK(!p4.chords[1].valid);
}

static void testGenerator()
{
    std::printf("[generator]\n");

    auto prog = parseProgression("Cmaj7 | Am7 | Dm7 G7 | Cmaj7");

    GeneratorSettings s;
    s.pattern = PatternId::BlockBeats;
    s.voicing.style = VoicingStyle::Close;
    s.voicing.voiceLeading = true;

    auto seq = generate(prog, s);
    CHECK(!seq.empty());

    // Sorted by time.
    for (size_t i = 1; i < seq.size(); ++i)
        CHECK(seq[i].startBeats >= seq[i - 1].startBeats - 1e-9);

    // All events fall within the progression length.
    for (auto& e : seq) {
        CHECK(e.startBeats >= 0.0);
        CHECK(e.startBeats < prog.totalBeats() + 1e-6);
        CHECK(e.note >= 0 && e.note <= 127);
        CHECK(e.velocity >= 0.0f && e.velocity <= 1.0f);
    }

    // Transpose shifts every note.
    GeneratorSettings st = s;
    st.transpose = 12;
    auto seqT = generate(prog, st);
    CHECK(seqT.size() == seq.size());
    for (size_t i = 0; i < seq.size(); ++i)
        CHECK(seqT[i].note == seq[i].note + 12);

    // Swing delays off-beat eighths.
    GeneratorSettings sw = s;
    sw.pattern = PatternId::ArpUp;
    sw.swing = 0.5;
    auto seqSwing = generate(prog, sw);
    GeneratorSettings straight = sw;
    straight.swing = 0.0;
    auto seqStraight = generate(prog, straight);
    CHECK(seqSwing.size() == seqStraight.size());

    // A real-time single-chord generation works too.
    std::vector<MidiNote> prev;
    auto live = generateChord(*parseChord("Fmaj7"), 0.0, 4.0, s, &prev);
    CHECK(!live.empty());
    CHECK(!prev.empty());
}

int main()
{
    std::printf("Running rhythm core tests...\n\n");
    testNoteHelpers();
    testChordParsing();
    testChordDetection();
    testVoicing();
    testPatterns();
    testProgression();
    testGenerator();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    if (g_failures == 0) std::printf("ALL TESTS PASSED\n");
    return g_failures == 0 ? 0 : 1;
}
