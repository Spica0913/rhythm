#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <set>
#include <vector>

#include "Generator.h"
#include "Progression.h"

// Parameter identifiers, shared between processor and editor.
namespace rhythm::params {
inline constexpr const char* mode         = "mode";
inline constexpr const char* voicing      = "voicing";
inline constexpr const char* pattern      = "pattern";
inline constexpr const char* octave       = "octave";
inline constexpr const char* voiceLeading = "voiceLeading";
inline constexpr const char* includeBass  = "includeBass";
inline constexpr const char* maxNotes     = "maxNotes";
inline constexpr const char* swing        = "swing";
inline constexpr const char* velocity     = "velocity";
inline constexpr const char* transpose    = "transpose";
inline constexpr const char* channel      = "channel";
inline constexpr const char* liveBars     = "liveBars";

// Non-parameter state stored in the value tree.
inline constexpr const char* progressionText = "progressionText";
inline constexpr const char* beatsPerBar     = "beatsPerBar";
} // namespace rhythm::params

// A MIDI-effect plugin that turns chord progressions (typed in the UI) or live
// MIDI chord input into voiced, rhythmic accompaniment.
class RhythmAudioProcessor : public juce::AudioProcessor,
                             private juce::AudioProcessorValueTreeState::Listener,
                             public juce::ValueTree::Listener
{
public:
    RhythmAudioProcessor();
    ~RhythmAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // Hosted as an instrument that emits MIDI (works in Ableton Live, FL,
    // Bitwig, etc., which do not have a third-party MIDI-effect slot). The
    // audio output is silent; the plugin's job is to generate MIDI for a
    // downstream instrument.
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // --- API used by the editor -------------------------------------------
    juce::AudioProcessorValueTreeState& state() { return apvts; }
    juce::ValueTree& valueTree() { return rootState; }

    // The most recently detected/active chord name (for display). Thread-safe.
    juce::String getCurrentChordName() const;

    // Mark the cached sequence dirty (call after editing the progression text).
    void invalidateSequence() { sequenceDirty.store(true); }

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    APVTS::ParameterLayout createLayout();
    void parameterChanged(const juce::String& id, float value) override;
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;

    rhythm::GeneratorSettings currentSettings() const;
    void rebuildSequenceIfNeeded();

    // Progression playback (transport-synced).
    void renderProgressionBlock(juce::MidiBuffer& out, int numSamples,
                                double bpm, double ppqStart, bool playing);
    // Live MIDI-input comping.
    void renderLiveBlock(juce::MidiBuffer& in, juce::MidiBuffer& out,
                         int numSamples, double bpm,
                         double ppqStart, bool playing);

    // A flattened note-on/note-off event used by the block scheduler.
    struct FlatEvent {
        double beat = 0.0; // within [0, loopLenBeats)
        bool on = false;
        int note = 60;
        int velocity = 100; // 0..127
        int channel = 1;    // 1..16
    };
    static void flatten(const rhythm::Sequence& seq, double loopLenBeats,
                        std::vector<FlatEvent>& out);

    void emitWindow(const std::vector<FlatEvent>& flat, double loopLenBeats,
                    juce::MidiBuffer& out, int numSamples,
                    double samplesPerBeat, double winStartBeat);
    void allNotesOff(juce::MidiBuffer& out, int sampleOffset = 0);

    juce::AudioProcessorValueTreeState apvts;
    juce::ValueTree rootState { "RhythmState" };

    double sr = 44100.0;

    // Cached generated sequence for progression mode.
    std::atomic<bool> sequenceDirty { true };
    rhythm::Sequence cachedSequence;
    std::vector<FlatEvent> cachedFlat;
    double cachedLoopBeats = 0.0;

    // Live-mode state.
    std::set<rhythm::MidiNote> heldNotes;
    rhythm::Chord liveChord;
    bool liveChordValid = false;
    rhythm::Sequence liveSequence;
    std::vector<FlatEvent> liveFlat;
    double liveLoopBeats = 4.0;
    std::vector<rhythm::MidiNote> liveVoiceLeadState;
    double freeRunBeats = 0.0; // internal clock when transport is stopped

    // Tracks notes we have turned on so we can turn them off cleanly.
    std::array<int, 16> activeNoteCount {}; // per-channel sanity (unused detail)
    std::set<int> soundingNotes;            // packed (channel<<8 | note)
    int lastMode = -1;                      // detect Progression<->Live switches

    juce::String currentChordName;
    mutable juce::CriticalSection chordNameLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RhythmAudioProcessor)
};
