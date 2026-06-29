#include "Chord.h"

#include <algorithm>
#include <map>
#include <set>

namespace rhythm {

namespace {

// Map a note letter to its base pitch class.
bool letterToPitchClass(char c, PitchClass& out)
{
    switch (std::toupper(static_cast<unsigned char>(c))) {
        case 'C': out = 0; return true;
        case 'D': out = 2; return true;
        case 'E': out = 4; return true;
        case 'F': out = 5; return true;
        case 'G': out = 7; return true;
        case 'A': out = 9; return true;
        case 'B': out = 11; return true;
        default: return false;
    }
}

// Replace common unicode accidentals/symbols with ASCII equivalents so the
// rest of the parser only deals with '#', 'b', and ASCII quality tokens.
std::string normaliseSymbol(const std::string& in)
{
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size();) {
        // Multi-byte UTF-8 sequences for musical accidentals/symbols.
        if (i + 2 < in.size()) {
            std::string three = in.substr(i, 3);
            if (three == "\xE2\x99\xAF") { out += '#'; i += 3; continue; } // ♯
            if (three == "\xE2\x99\xAD") { out += 'b'; i += 3; continue; } // ♭
            if (three == "\xCE\x94")     { } // handled below (2-byte)
        }
        if (i + 1 < in.size()) {
            std::string two = in.substr(i, 2);
            if (two == "\xCE\x94") { out += "maj7"; i += 2; continue; } // Δ
            if (two == "\xC2\xB0") { out += "dim";  i += 2; continue; } // °
            if (two == "\xC3\xB8") { out += "m7b5"; i += 2; continue; } // ø
        }
        char c = in[i];
        if (!std::isspace(static_cast<unsigned char>(c)))
            out += c;
        ++i;
    }
    return out;
}

// Parse a note name (letter + accidentals) starting at pos; advances pos.
bool parseNoteName(const std::string& s, size_t& pos, PitchClass& out)
{
    if (pos >= s.size()) return false;
    PitchClass pc = 0;
    if (!letterToPitchClass(s[pos], pc)) return false;
    ++pos;
    while (pos < s.size() && (s[pos] == '#' || s[pos] == 'b')) {
        pc += (s[pos] == '#') ? 1 : -1;
        ++pos;
    }
    out = mod12(pc);
    return true;
}

// Returns true if `q` contains `token` (used for alterations like "b9").
bool has(const std::string& q, const std::string& token)
{
    return q.find(token) != std::string::npos;
}

// Add an interval to the set if not already present.
void add(std::set<int>& set, int interval) { set.insert(interval); }

} // namespace

std::vector<PitchClass> Chord::pitchClasses() const
{
    std::set<PitchClass> set;
    set.insert(root);
    for (int iv : intervals)
        set.insert(mod12(root + iv));
    return { set.begin(), set.end() };
}

std::optional<Chord> parseChord(const std::string& rawSymbol)
{
    std::string symbol = normaliseSymbol(rawSymbol);
    if (symbol.empty()) return std::nullopt;

    Chord chord;
    chord.symbol = rawSymbol;

    size_t pos = 0;
    if (!parseNoteName(symbol, pos, chord.root)) return std::nullopt;

    // Split off the slash bass, if present.
    std::string quality = symbol.substr(pos);
    auto slash = quality.find('/');
    if (slash != std::string::npos) {
        std::string bassStr = quality.substr(slash + 1);
        quality = quality.substr(0, slash);
        size_t bp = 0;
        PitchClass bassPc = 0;
        if (parseNoteName(bassStr, bp, bassPc) && bp == bassStr.size())
            chord.bass = bassPc;
        else
            return std::nullopt;
    }

    std::set<int> ivs;

    // --- Determine the triad / base quality -------------------------------
    bool isMinor = false, isDim = false, isAug = false, isSus2 = false,
         isSus4 = false, isPower = false, isMajSeventhFamily = false;

    // Longest-prefix style checks. Order matters.
    auto startsWith = [&](const std::string& p) {
        return quality.rfind(p, 0) == 0;
    };

    // Power chord "5" with nothing else.
    if (quality == "5") {
        isPower = true;
    } else if (startsWith("maj") || startsWith("Maj") || startsWith("MAJ")) {
        isMajSeventhFamily = true; // major family; 7th (if any) is major
    } else if (!quality.empty() && quality[0] == 'M' &&
               (quality.size() == 1 || std::isdigit((unsigned char)quality[1]))) {
        // "M", "M7", "M9" -> major seventh family
        isMajSeventhFamily = true;
    } else if (startsWith("dim") || startsWith("o")) {
        isDim = true;
    } else if (startsWith("aug")) {
        isAug = true;
    } else if (startsWith("min") || startsWith("-") ||
               (!quality.empty() && quality[0] == 'm' && !startsWith("maj"))) {
        isMinor = true;
    }

    if (has(quality, "sus2")) isSus2 = true;
    else if (has(quality, "sus")) isSus4 = true; // "sus" or "sus4"

    // Triad construction.
    if (isPower) {
        add(ivs, kPerf5);
    } else if (isSus2) {
        add(ivs, kMaj2); add(ivs, kPerf5);
    } else if (isSus4) {
        add(ivs, kPerf4); add(ivs, kPerf5);
    } else if (isDim) {
        add(ivs, kMin3); add(ivs, kDim5);
    } else if (isAug) {
        add(ivs, kMaj3); add(ivs, kAug5);
    } else if (isMinor) {
        add(ivs, kMin3); add(ivs, kPerf5);
    } else {
        add(ivs, kMaj3); add(ivs, kPerf5);
    }

    // --- Sevenths and extensions ------------------------------------------
    // Detect the highest stacked extension number (7/9/11/13) and the 6th.
    auto hasNumber = [&](const std::string& n) {
        // Look for the number not immediately preceded by '#'/'b' (those are
        // alterations like #11/b13, handled separately).
        size_t p = quality.find(n);
        while (p != std::string::npos) {
            bool altered = (p > 0 && (quality[p - 1] == '#' || quality[p - 1] == 'b'));
            // Avoid matching the "1" inside "11"/"13" when searching for "1".
            if (!altered) return true;
            p = quality.find(n, p + 1);
        }
        return false;
    };

    bool dim7 = isDim && (has(quality, "dim7") || has(quality, "o7") ||
                          (quality == "dim7"));
    bool half = has(quality, "m7b5") || has(quality, "7b5");

    bool sixth = hasNumber("6") && !has(quality, "b6");
    bool add9only = has(quality, "add9");
    bool sixNine = has(quality, "6/9") || has(quality, "69");

    int topExt = 0; // 7, 9, 11, or 13
    if (hasNumber("13")) topExt = 13;
    else if (hasNumber("11")) topExt = 11;
    else if (hasNumber("9")) topExt = 9;
    else if (hasNumber("7")) topExt = 7;

    // Seventh quality.
    if (dim7 || (isDim && topExt == 7)) {
        // dim7 uses a diminished (double-flat) 7th = major 6th interval.
        add(ivs, kMaj6);
    } else if (topExt >= 7 || sixNine) {
        if (isMajSeventhFamily) add(ivs, kMaj7);
        else if (sixNine) { /* no 7th for 6/9 */ }
        else add(ivs, kMin7); // dominant or minor seventh
    }
    if (half) { // m7b5 already minor; ensure dim5 + min7
        ivs.erase(kPerf5);
        add(ivs, kDim5);
        add(ivs, kMin7);
    }

    // Upper extensions stack (jazz convention). The natural 11th is only
    // stacked when explicitly an 11 chord, or on minor chords; on major/
    // dominant 13 chords it is omitted because it clashes with the major 3rd.
    bool minorThird = ivs.count(kMin3) > 0;
    if (topExt >= 9 || add9only || sixNine) add(ivs, kNat9);
    if (topExt == 11 || (topExt >= 11 && minorThird)) add(ivs, kNat11);
    if (topExt >= 13) add(ivs, kNat13);

    // Sixth (and 6/9).
    if (sixth || sixNine) add(ivs, kMaj6);

    // --- Alterations -------------------------------------------------------
    if (has(quality, "b5")) { ivs.erase(kPerf5); add(ivs, kDim5); }
    if (has(quality, "#5") || has(quality, "+5")) { ivs.erase(kPerf5); add(ivs, kAug5); }
    if (has(quality, "b9")) add(ivs, kFlat9);
    if (has(quality, "#9")) add(ivs, kSharp9);
    if (has(quality, "#11")) add(ivs, kSharp11);
    if (has(quality, "b13")) add(ivs, kFlat13);
    if (has(quality, "alt")) { // altered dominant: b9 #9 b5 #5
        add(ivs, kFlat9); add(ivs, kSharp9);
        ivs.erase(kPerf5); add(ivs, kDim5); add(ivs, kAug5);
    }

    if (ivs.empty()) return std::nullopt;

    chord.intervals.assign(ivs.begin(), ivs.end());
    std::sort(chord.intervals.begin(), chord.intervals.end());
    return chord;
}

// ---------------------------------------------------------------------------
// Chord detection from sounding notes.
// ---------------------------------------------------------------------------
namespace {

// Candidate chord templates as interval sets (from root, mod-12 reduced).
struct Template {
    const char* suffix;
    std::vector<int> intervals; // not including root
    int score;                  // tie-break preference (higher = preferred)
};

const std::vector<Template>& templates()
{
    static const std::vector<Template> t = {
        { "maj7", { kMaj3, kPerf5, kMaj7 }, 9 },
        { "7",    { kMaj3, kPerf5, kMin7 }, 9 },
        { "m7",   { kMin3, kPerf5, kMin7 }, 9 },
        { "m7b5", { kMin3, kDim5, kMin7 }, 8 },
        { "dim7", { kMin3, kDim5, kMaj6 }, 8 },
        { "6",    { kMaj3, kPerf5, kMaj6 }, 7 },
        { "m6",   { kMin3, kPerf5, kMaj6 }, 7 },
        { "mMaj7",{ kMin3, kPerf5, kMaj7 }, 6 },
        { "",     { kMaj3, kPerf5 }, 5 },        // major triad
        { "m",    { kMin3, kPerf5 }, 5 },        // minor triad
        { "dim",  { kMin3, kDim5 }, 4 },
        { "aug",  { kMaj3, kAug5 }, 4 },
        { "sus4", { kPerf4, kPerf5 }, 3 },
        { "sus2", { kMaj2, kPerf5 }, 3 },
        { "5",    { kPerf5 }, 2 },
    };
    return t;
}

} // namespace

std::optional<Chord> detectChord(const std::vector<MidiNote>& notes)
{
    if (notes.size() < 2) return std::nullopt;

    std::set<PitchClass> pcSet;
    for (auto n : notes) pcSet.insert(pitchClassOf(n));
    std::vector<PitchClass> pcs(pcSet.begin(), pcSet.end());

    MidiNote lowest = *std::min_element(notes.begin(), notes.end());
    PitchClass bassPc = pitchClassOf(lowest);

    const Chord* dummy = nullptr; (void)dummy;

    Chord best;
    int bestScore = -1;
    bool found = false;

    // Try each pitch class as a potential root.
    for (PitchClass root : pcs) {
        std::set<int> rel; // intervals relative to this root (mod 12)
        for (PitchClass pc : pcs)
            rel.insert(mod12(pc - root));

        for (const auto& tmpl : templates()) {
            std::set<int> tmplSet(tmpl.intervals.begin(), tmpl.intervals.end());
            tmplSet.insert(kRoot);

            // Count how many template tones are present and how many extra.
            int matched = 0;
            for (int iv : tmplSet)
                if (rel.count(iv)) ++matched;

            if (matched < (int)tmplSet.size()) continue; // template not fully present

            int extra = (int)rel.size() - (int)tmplSet.size();
            // Score: prefer exact matches, then template preference, then
            // roots that match the bass note.
            int score = matched * 10 - extra * 3 + tmpl.score;
            if (root == bassPc) score += 4;

            if (score > bestScore) {
                bestScore = score;
                best.root = root;
                best.intervals.assign(tmplSet.begin(), tmplSet.end());
                best.intervals.erase(
                    std::remove(best.intervals.begin(), best.intervals.end(), 0),
                    best.intervals.end());
                best.bass = (bassPc != root) ? std::optional<PitchClass>(bassPc)
                                             : std::nullopt;
                best.symbol = chordToString(best);
                found = true;
            }
        }
    }

    return found ? std::optional<Chord>(best) : std::nullopt;
}

std::string chordToString(const Chord& chord, bool preferFlats)
{
    const auto& names = preferFlats ? flatNoteNames() : sharpNoteNames();
    std::string out = names[mod12(chord.root)];

    std::set<int> ivs(chord.intervals.begin(), chord.intervals.end());
    auto hasIv = [&](int i) { return ivs.count(i) > 0; };

    bool min3 = hasIv(kMin3), maj3 = hasIv(kMaj3);
    bool dim5 = hasIv(kDim5), aug5 = hasIv(kAug5);
    bool min7 = hasIv(kMin7), maj7 = hasIv(kMaj7), dim7 = hasIv(kMaj6) && min3 && dim5;

    if (min3 && dim5 && min7) { out += "m7b5"; }
    else if (min3 && dim5 && hasIv(kMaj6)) { out += "dim7"; }
    else if (min3 && dim5) { out += "dim"; }
    else if (maj3 && aug5) { out += "aug"; }
    else if (min3) {
        out += "m";
        if (maj7) out += "Maj7"; else if (min7) out += "7";
        else if (hasIv(kMaj6)) out += "6";
    } else { // major-ish
        if (maj7) out += "maj7"; else if (min7) out += "7";
        else if (hasIv(kMaj6)) out += "6";
        else if (!maj3 && hasIv(kPerf4)) out += "sus4";
        else if (!maj3 && hasIv(kMaj2)) out += "sus2";
    }

    // Extensions.
    if (hasIv(kNat9))   out += "9";
    if (hasIv(kFlat9))  out += "b9";
    if (hasIv(kSharp9)) out += "#9";
    if (hasIv(kNat11))  out += "11";
    if (hasIv(kSharp11))out += "#11";
    if (hasIv(kNat13))  out += "13";
    if (hasIv(kFlat13)) out += "b13";

    if (chord.bass && *chord.bass != chord.root)
        out += "/" + names[mod12(*chord.bass)];

    (void)dim7;
    return out;
}

} // namespace rhythm
