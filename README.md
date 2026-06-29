# Rhythm

A VST3/AU instrument plugin (built with [JUCE](https://juce.com)) that turns
**chord progressions** into **voicings**, **accompaniment patterns**, and
**rhythm patterns** — for any chord progression you throw at it.

It is hosted as an **instrument that generates MIDI** (rather than audio): you
load it on a track and route its MIDI output into the sound you actually want to
hear. This is the form that works in Ableton Live, FL Studio and Bitwig, which
have no third-party MIDI-effect slot.

You can drive it two ways:

1. **Progression mode** — type a lead-sheet style progression
   (`Cmaj7 | Am7 | Dm7 G7`) into the plugin and it plays voiced, rhythmic
   accompaniment locked to your DAW's transport.
2. **Live mode** — play/hold chords on a MIDI keyboard; the plugin detects the
   chord and re-voices + comps it in real time.

The whole musical brain lives in a small, **JUCE-free C++ core** (`src/core`)
that is fully unit-tested and can be used on its own — including a command-line
tool (`rhythm_cli`) that exports accompaniment straight to a `.mid` file with no
DAW required.

---

## Features

**Chord understanding**
- A robust chord-symbol parser: triads, `6 m6 maj7 7 m7 m7b5 dim7 mMaj7`,
  extensions `9 11 13` (with jazz stacking conventions), alterations
  `b5 #5 b9 #9 #11 b13 alt`, `sus2/sus4`, power chords, and slash chords (`C/E`).
  Unicode `♯ ♭ Δ ° ø` are understood too.
- Chord **detection** from held MIDI notes (for Live mode).

**Voicings** (`VoicingStyle`)
- Close, Drop 2, Drop 3, Drop 2+4, Shell (R-3-7), Rootless A (3-5-7-9),
  Rootless B (7-9-3-5), Open, Power/Octave.
- Optional **voice leading** that keeps successive chords close together.
- Optional separate **bass note**, register/octave control, and a voice-count cap.

**Accompaniment / rhythm patterns** (`PatternId`)
- Block (whole / per-beat), Off-beats, Arp up / down / up-down, Alberti bass,
  Ballad, Pop comp, Bossa nova, Waltz (3/4), Off-beat 8ths.
- A **swing** control and global velocity scaling.
- Patterns are tiled across each chord's duration, so a one-bar pattern fills a
  four-bar chord automatically.

---

## Project layout

```
src/core/        Pure C++ engine (no JUCE) — chords, voicings, patterns, generator
  Chord.*          symbol parsing + chord detection
  Voicing.*        voicing strategies + voice leading
  Pattern.*        accompaniment / rhythm patterns
  Progression.*    lead-sheet progression parser
  Generator.*      progression -> timed MIDI sequence
  MidiFile.*       minimal Standard MIDI File writer
src/plugin/      JUCE plugin (AudioProcessor + editor GUI)
tools/           rhythm_cli — export a progression to a .mid file
tests/           dependency-free unit tests for the core
```

---

## Building

Requires CMake ≥ 3.22 and a C++17 compiler.

### Core engine, tests, and the CLI (no JUCE needed)

```bash
cmake -S . -B build -DRHYTHM_BUILD_PLUGIN=OFF
cmake --build build -j
ctest --test-dir build --output-on-failure
```

### The plugin (VST3 / AU / Standalone)

The plugin needs JUCE. By default CMake fetches it from GitHub:

```bash
cmake -S . -B build            # downloads JUCE 8.0.4 via FetchContent
cmake --build build -j
```

If your environment can't reach GitHub (or you already have JUCE), point CMake
at a local checkout instead:

```bash
git clone --branch 8.0.4 --depth 1 https://github.com/juce-framework/JUCE.git ../JUCE
cmake -S . -B build -DJUCE_PATH=../JUCE
cmake --build build -j
```

Built plug-ins land under `build/RhythmPlugin_artefacts/`. The build is set to
copy them into your user plugin folder automatically (`COPY_PLUGIN_AFTER_BUILD`);
on macOS that is `~/Library/Audio/Plug-Ins/VST3` and `.../Components`.

### Using it in a DAW

Rhythm produces **MIDI**, not sound, so chain it into an instrument:

**Ableton Live**
1. On a MIDI track, load **Rhythm** as the instrument. Type your progression
   and pick a voicing/pattern.
2. Add a second MIDI track with the sound you want (e.g. a piano/synth).
3. On that second track set **MIDI From → (the Rhythm track)**, and in the box
   just below choose the **Rhythm** plugin as the sub-source.
4. Set the second track's **Monitor** to **In**, and arm it.
5. Press play — Rhythm follows Live's transport (Progression mode), or play
   chords into the Rhythm track to comp them live (Live mode).

**Logic Pro:** load the AU on an instrument track; route its MIDI to another
track via the environment/IAC, or use the bundled `rhythm_cli` to export MIDI.

**Quick alternative (no plugin needed):** export a `.mid` with `rhythm_cli`
(below) and drag it onto a track.

> **Note:** this repository was developed in a sandbox whose network policy
> blocks `github.com`, so the JUCE plugin target could not be compiled here.
> The **core engine is fully built and tested** in that sandbox (see CI / the
> 475 passing checks), and the plugin code targets the JUCE 8 API. Build the
> plugin on a machine with JUCE access using the steps above.

---

## CLI usage

`rhythm_cli` generates a MIDI file from a progression — handy for auditioning
voicings/patterns or producing parts without opening a DAW.

```bash
./build/rhythm_cli --prog "Dm7 | G7 | Cmaj7 | Cmaj7" \
                   --voicing drop2 --pattern bossa --bass --out bossa.mid

./build/rhythm_cli --prog "C G | Am F" --voicing close --pattern popcomp
./build/rhythm_cli --list          # list all voicings and patterns
./build/rhythm_cli --help
```

Options: `--prog --voicing --pattern --bpm --octave --bass --no-voiceleading
--swing --out`.

---

## Progression syntax

- Bars are separated by `|`; chords within a bar are separated by spaces and
  split the bar evenly.
- `%` (or `/`) repeats the previous chord.
- Unrecognised tokens become rests.

```
Cmaj7 | Am7 | Dm7 G7 | Cmaj7        # one chord per bar, two in bar 3
| C  G | Am F |                     # leading/trailing | are ignored
C | %                              # bar 2 repeats C
```

---

## Status

- Core engine: **built and tested** (`ctest`) — 475 assertions covering chord
  parsing, detection, voicings, voice leading, patterns, progression parsing,
  the generator, and MIDI-file output.
- Plugin: code complete against the JUCE 8 API; build where JUCE is reachable.
