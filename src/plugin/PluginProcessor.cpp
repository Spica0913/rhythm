#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace rhythm;

namespace {

// Choice lists for the combo-box parameters (order is the stored index).
const juce::StringArray kModeChoices    { "Progression", "Live (MIDI in)" };
const juce::StringArray kVoicingChoices {
    "Close", "Drop 2", "Drop 3", "Drop 2+4", "Shell",
    "Rootless A", "Rootless B", "Open", "Power/Octave"
};
const juce::StringArray kPatternChoices {
    "Block (whole)", "Block (beats)", "Off-beats", "Arp up", "Arp down",
    "Arp up/down", "Alberti", "Ballad", "Pop comp", "Bossa", "Waltz",
    "Off-beat 8ths"
};

VoicingStyle voicingFromIndex(int i)
{
    static const VoicingStyle map[] = {
        VoicingStyle::Close, VoicingStyle::Drop2, VoicingStyle::Drop3,
        VoicingStyle::Drop24, VoicingStyle::Shell, VoicingStyle::RootlessA,
        VoicingStyle::RootlessB, VoicingStyle::Open, VoicingStyle::PowerOctave
    };
    return map[juce::jlimit(0, (int)std::size(map) - 1, i)];
}

PatternId patternFromIndex(int i)
{
    static const PatternId map[] = {
        PatternId::BlockWhole, PatternId::BlockBeats, PatternId::OffBeats,
        PatternId::ArpUp, PatternId::ArpDown, PatternId::ArpUpDown,
        PatternId::Alberti, PatternId::Ballad, PatternId::PopComp,
        PatternId::Bossa, PatternId::Waltz, PatternId::Offbeat8
    };
    return map[juce::jlimit(0, (int)std::size(map) - 1, i)];
}

} // namespace

RhythmAudioProcessor::RhythmAudioProcessor()
    : AudioProcessor(BusesProperties() // instrument: silent stereo output
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createLayout())
{
    // Default progression and meter live in the value tree as properties.
    rootState.setProperty(params::progressionText,
                          "Cmaj7 | Am7 | Dm7 | G7", nullptr);
    rootState.setProperty(params::beatsPerBar, 4.0, nullptr);
    rootState.addListener(this);

    for (auto* id : { params::mode, params::voicing, params::pattern,
                      params::octave, params::voiceLeading, params::includeBass,
                      params::maxNotes, params::swing, params::velocity,
                      params::transpose, params::channel, params::liveBars })
        apvts.addParameterListener(id, this);
}

RhythmAudioProcessor::~RhythmAudioProcessor()
{
    rootState.removeListener(this);
}

juce::AudioProcessorValueTreeState::ParameterLayout
RhythmAudioProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID { params::mode, 1 }, "Mode", kModeChoices, 0));
    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID { params::voicing, 1 }, "Voicing", kVoicingChoices, 0));
    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID { params::pattern, 1 }, "Pattern", kPatternChoices, 1));
    p.push_back(std::make_unique<AudioParameterInt>(
        ParameterID { params::octave, 1 }, "Octave", -2, 3, 0));
    p.push_back(std::make_unique<AudioParameterBool>(
        ParameterID { params::voiceLeading, 1 }, "Voice leading", true));
    p.push_back(std::make_unique<AudioParameterBool>(
        ParameterID { params::includeBass, 1 }, "Bass note", false));
    p.push_back(std::make_unique<AudioParameterInt>(
        ParameterID { params::maxNotes, 1 }, "Max notes", 2, 6, 5));
    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID { params::swing, 1 }, "Swing",
        NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID { params::velocity, 1 }, "Velocity",
        NormalisableRange<float>(0.1f, 1.5f, 0.01f), 1.0f));
    p.push_back(std::make_unique<AudioParameterInt>(
        ParameterID { params::transpose, 1 }, "Transpose", -24, 24, 0));
    p.push_back(std::make_unique<AudioParameterInt>(
        ParameterID { params::channel, 1 }, "MIDI channel", 1, 16, 1));
    p.push_back(std::make_unique<AudioParameterInt>(
        ParameterID { params::liveBars, 1 }, "Live loop (bars)", 1, 8, 1));

    return { p.begin(), p.end() };
}

void RhythmAudioProcessor::parameterChanged(const juce::String&, float)
{
    // Any parameter change can affect generation; rebuild lazily.
    sequenceDirty.store(true);
}

void RhythmAudioProcessor::valueTreePropertyChanged(juce::ValueTree&,
                                                    const juce::Identifier&)
{
    sequenceDirty.store(true);
}

bool RhythmAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Accept a mono or stereo (silent) output; the plugin generates MIDI, not audio.
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        || out == juce::AudioChannelSet::mono()
        || out == juce::AudioChannelSet::disabled();
}

void RhythmAudioProcessor::prepareToPlay(double sampleRate, int)
{
    sr = sampleRate;
    sequenceDirty.store(true);
    soundingNotes.clear();
    heldNotes.clear();
    liveVoiceLeadState.clear();
    freeRunBeats = 0.0;
}

GeneratorSettings RhythmAudioProcessor::currentSettings() const
{
    GeneratorSettings s;
    auto get = [&](const char* id) {
        return apvts.getRawParameterValue(id)->load();
    };

    s.voicing.style = voicingFromIndex((int)get(params::voicing));
    s.voicing.centerNote = kMiddleC + 12 * (int)get(params::octave);
    s.voicing.voiceLeading = get(params::voiceLeading) > 0.5f;
    s.voicing.includeBass = get(params::includeBass) > 0.5f;
    s.voicing.maxNotes = (int)get(params::maxNotes);
    s.pattern = patternFromIndex((int)get(params::pattern));
    s.swing = get(params::swing);
    s.velocityScale = get(params::velocity);
    s.transpose = (int)get(params::transpose);
    s.midiChannel = (int)get(params::channel);
    return s;
}

void RhythmAudioProcessor::flatten(const Sequence& seq, double loopLenBeats,
                                   std::vector<FlatEvent>& out)
{
    out.clear();
    out.reserve(seq.size() * 2);
    for (const auto& e : seq) {
        double on = e.startBeats;
        double off = e.endBeats();
        if (loopLenBeats > 0.0) off = std::min(off, loopLenBeats); // clip to loop
        int vel = juce::jlimit(1, 127, (int)std::lround(e.velocity * 127.0f));
        out.push_back({ on, true, e.note, vel, e.channel });
        out.push_back({ off, false, e.note, vel, e.channel });
    }
    std::sort(out.begin(), out.end(), [](const FlatEvent& a, const FlatEvent& b) {
        if (a.beat != b.beat) return a.beat < b.beat;
        return (a.on ? 1 : 0) < (b.on ? 1 : 0); // note-offs before note-ons at same time
    });
}

void RhythmAudioProcessor::rebuildSequenceIfNeeded()
{
    if (!sequenceDirty.exchange(false)) return;

    auto settings = currentSettings();
    auto text = rootState.getProperty(params::progressionText).toString();
    double bpb = (double)rootState.getProperty(params::beatsPerBar, 4.0);

    auto prog = parseProgression(text.toStdString(), bpb);
    cachedSequence = generate(prog, settings);
    cachedLoopBeats = prog.totalBeats();
    if (cachedLoopBeats <= 0.0) cachedLoopBeats = bpb; // avoid div-by-zero
    flatten(cachedSequence, cachedLoopBeats, cachedFlat);

    // Update display name with the first valid chord.
    juce::String name;
    for (const auto& c : prog.chords)
        if (c.valid) { name = juce::String(chordToString(c.chord)); break; }
    {
        const juce::ScopedLock sl(chordNameLock);
        currentChordName = name;
    }
}

void RhythmAudioProcessor::allNotesOff(juce::MidiBuffer& out, int sampleOffset)
{
    for (int packed : soundingNotes) {
        int ch = (packed >> 8) & 0xff;
        int note = packed & 0xff;
        out.addEvent(juce::MidiMessage::noteOff(ch, note), sampleOffset);
    }
    soundingNotes.clear();
}

void RhythmAudioProcessor::emitWindow(const std::vector<FlatEvent>& flat,
                                      double loopLenBeats, juce::MidiBuffer& out,
                                      int numSamples, double samplesPerBeat,
                                      double winStartBeat)
{
    if (flat.empty() || loopLenBeats <= 0.0 || samplesPerBeat <= 0.0) return;

    const double blockBeats = numSamples / samplesPerBeat;
    const double winEnd = winStartBeat + blockBeats;

    for (const auto& e : flat) {
        // First occurrence of this event at or after the window start.
        double k = std::ceil((winStartBeat - e.beat) / loopLenBeats);
        double occ = e.beat + k * loopLenBeats;
        while (occ < winEnd - 1e-9) {
            int sample = (int)std::lround((occ - winStartBeat) * samplesPerBeat);
            sample = juce::jlimit(0, numSamples - 1, sample);
            int ch = juce::jlimit(1, 16, e.channel);
            int packed = (ch << 8) | (e.note & 0xff);
            if (e.on) {
                out.addEvent(juce::MidiMessage::noteOn(ch, e.note,
                                                       (juce::uint8)e.velocity),
                             sample);
                soundingNotes.insert(packed);
            } else {
                out.addEvent(juce::MidiMessage::noteOff(ch, e.note), sample);
                soundingNotes.erase(packed);
            }
            occ += loopLenBeats;
        }
    }
}

void RhythmAudioProcessor::renderProgressionBlock(juce::MidiBuffer& out,
                                                  int numSamples, double bpm,
                                                  double ppqStart, bool playing)
{
    if (!playing) { allNotesOff(out); return; }
    const double samplesPerBeat = (sr * 60.0) / bpm;
    emitWindow(cachedFlat, cachedLoopBeats, out, numSamples,
               samplesPerBeat, ppqStart);
}

void RhythmAudioProcessor::renderLiveBlock(juce::MidiBuffer& in,
                                           juce::MidiBuffer& out, int numSamples,
                                           double bpm, double ppqStart,
                                           bool playing)
{
    // Track held notes from incoming MIDI; consume the note events so they
    // don't pass straight through.
    bool chordChanged = false;
    for (const auto meta : in) {
        const auto m = meta.getMessage();
        if (m.isNoteOn()) { heldNotes.insert(m.getNoteNumber()); chordChanged = true; }
        else if (m.isNoteOff()) { heldNotes.erase(m.getNoteNumber()); chordChanged = true; }
        else if (m.isAllNotesOff() || m.isAllSoundOff()) { heldNotes.clear(); chordChanged = true; }
        else out.addEvent(m, meta.samplePosition); // pass through CC/PB etc.
    }

    if (heldNotes.empty()) {
        allNotesOff(out);
        liveChordValid = false;
        {
            const juce::ScopedLock sl(chordNameLock);
            currentChordName = {};
        }
        return;
    }

    if (chordChanged) {
        std::vector<MidiNote> notes(heldNotes.begin(), heldNotes.end());
        if (auto c = detectChord(notes)) {
            // Only re-voice when the detected chord actually differs, so that
            // simply adding/removing a doubled note doesn't restart the comp.
            bool isNew = !liveChordValid
                         || c->root != liveChord.root
                         || c->bass != liveChord.bass
                         || c->pitchClasses() != liveChord.pitchClasses();
            if (isNew) {
                // Stop the previous voicing's notes cleanly before switching.
                allNotesOff(out);
                liveChord = *c;
                liveChordValid = true;
                auto settings = currentSettings();
                double bars = apvts.getRawParameterValue(params::liveBars)->load();
                double bpb = (double)rootState.getProperty(params::beatsPerBar, 4.0);
                liveLoopBeats = bars * bpb;
                liveSequence = generateChord(liveChord, 0.0, liveLoopBeats,
                                             settings, &liveVoiceLeadState);
                if (settings.swing > 0.0) applySwing(liveSequence, settings.swing);
                flatten(liveSequence, liveLoopBeats, liveFlat);
                {
                    const juce::ScopedLock sl(chordNameLock);
                    currentChordName = juce::String(chordToString(liveChord));
                }
            }
        }
    }

    if (!liveChordValid) return;

    const double samplesPerBeat = (sr * 60.0) / bpm;
    // When the host is stopped we free-run an internal clock so live jamming
    // still produces rhythm; when playing we lock to the transport.
    double winStart = playing ? ppqStart : freeRunBeats;
    emitWindow(liveFlat, liveLoopBeats, out, numSamples, samplesPerBeat, winStart);
    if (!playing)
        freeRunBeats += numSamples / samplesPerBeat;
}

void RhythmAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    rebuildSequenceIfNeeded();

    // Transport info.
    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
    if (auto* ph = getPlayHead()) {
        if (auto pos = ph->getPosition()) {
            if (auto t = pos->getBpm())          bpm = *t;
            if (auto q = pos->getPpqPosition())  ppq = *q;
            playing = pos->getIsPlaying();
        }
    }
    if (bpm <= 0.0) bpm = 120.0;

    const int mode = (int)apvts.getRawParameterValue(params::mode)->load();

    juce::MidiBuffer generated;
    if (mode != lastMode) {
        allNotesOff(generated);      // flush any hanging notes on mode change
        liveChordValid = false;
        lastMode = mode;
    }

    if (mode == 1) {
        renderLiveBlock(midi, generated, buffer.getNumSamples(), bpm, ppq, playing);
    } else {
        renderProgressionBlock(generated, buffer.getNumSamples(), bpm, ppq, playing);
    }

    midi.swapWith(generated);
}

juce::AudioProcessorEditor* RhythmAudioProcessor::createEditor()
{
    return new RhythmAudioProcessorEditor(*this);
}

juce::String RhythmAudioProcessor::getCurrentChordName() const
{
    const juce::ScopedLock sl(chordNameLock);
    return currentChordName;
}

void RhythmAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    // Combine the parameter tree and our property tree.
    juce::ValueTree combined("RhythmPlugin");
    combined.appendChild(apvts.copyState(), nullptr);
    combined.appendChild(rootState.createCopy(), nullptr);
    if (auto xml = combined.createXml())
        copyXmlToBinary(*xml, dest);
}

void RhythmAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size)) {
        juce::ValueTree combined = juce::ValueTree::fromXml(*xml);
        if (combined.isValid()) {
            auto pstate = combined.getChildWithName("PARAMS");
            if (pstate.isValid()) apvts.replaceState(pstate);
            auto rstate = combined.getChildWithName("RhythmState");
            if (rstate.isValid()) {
                rootState.removeListener(this);
                rootState.copyPropertiesAndChildrenFrom(rstate, nullptr);
                rootState.addListener(this);
            }
        }
    }
    sequenceDirty.store(true);
}

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RhythmAudioProcessor();
}
