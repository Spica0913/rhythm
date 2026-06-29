#include "PluginEditor.h"

namespace {
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
constexpr int kWidth = 640;
constexpr int kHeight = 460;
} // namespace

RhythmAudioProcessorEditor::RhythmAudioProcessorEditor(RhythmAudioProcessor& p)
    : AudioProcessorEditor(&p), proc(p)
{
    titleLabel.setText("RHYTHM — voicing & accompaniment", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(20.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    chordReadout.setFont(juce::Font(18.0f, juce::Font::bold));
    chordReadout.setJustificationType(juce::Justification::centredRight);
    chordReadout.setColour(juce::Label::textColourId, juce::Colours::aqua);
    addAndMakeVisible(chordReadout);

    // Progression text entry.
    progLabel.setText("Chord progression  (bars separated by | , chords by spaces)",
                      juce::dontSendNotification);
    addAndMakeVisible(progLabel);

    progressionEditor.setMultiLine(true, true);
    progressionEditor.setReturnKeyStartsNewLine(true);
    progressionEditor.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(),
                                         15.0f, juce::Font::plain));
    progressionEditor.setText(
        proc.valueTree().getProperty(rhythm::params::progressionText).toString(),
        false);
    progressionEditor.onTextChange = [this] { pushProgressionText(); };
    progressionEditor.onFocusLost = [this] { pushProgressionText(); };
    addAndMakeVisible(progressionEditor);

    addCombo(modeBox, modeLabel, "Mode", kModeChoices, rhythm::params::mode, modeAttach);
    addCombo(voicingBox, voicingLabel, "Voicing", kVoicingChoices,
             rhythm::params::voicing, voicingAttach);
    addCombo(patternBox, patternLabel, "Pattern", kPatternChoices,
             rhythm::params::pattern, patternAttach);

    addSlider(octaveSlider, octaveLabel, "Octave", rhythm::params::octave, octaveAttach);
    addSlider(maxNotesSlider, maxNotesLabel, "Max notes", rhythm::params::maxNotes, maxNotesAttach);
    addSlider(swingSlider, swingLabel, "Swing", rhythm::params::swing, swingAttach);
    addSlider(velocitySlider, velocityLabel, "Velocity", rhythm::params::velocity, velocityAttach);
    addSlider(transposeSlider, transposeLabel, "Transpose", rhythm::params::transpose, transposeAttach);
    addSlider(channelSlider, channelLabel, "Channel", rhythm::params::channel, channelAttach);
    addSlider(liveBarsSlider, liveBarsLabel, "Live bars", rhythm::params::liveBars, liveBarsAttach);

    addAndMakeVisible(voiceLeadingButton);
    addAndMakeVisible(bassButton);
    voiceLeadingAttach = std::make_unique<ButtonAttach>(
        proc.state(), rhythm::params::voiceLeading, voiceLeadingButton);
    bassAttach = std::make_unique<ButtonAttach>(
        proc.state(), rhythm::params::includeBass, bassButton);

    setSize(kWidth, kHeight);
    startTimerHz(15);
}

RhythmAudioProcessorEditor::~RhythmAudioProcessorEditor() { stopTimer(); }

void RhythmAudioProcessorEditor::addCombo(juce::ComboBox& c, juce::Label& l,
                                          const juce::String& text,
                                          const juce::StringArray& items,
                                          const char* paramId,
                                          std::unique_ptr<ComboAttach>& attach)
{
    l.setText(text, juce::dontSendNotification);
    l.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(l);
    c.addItemList(items, 1);
    addAndMakeVisible(c);
    attach = std::make_unique<ComboAttach>(proc.state(), paramId, c);
}

void RhythmAudioProcessorEditor::addSlider(juce::Slider& s, juce::Label& l,
                                           const juce::String& text,
                                           const char* paramId,
                                           std::unique_ptr<SliderAttach>& attach)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 16);
    addAndMakeVisible(s);
    l.setText(text, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    l.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(l);
    attach = std::make_unique<SliderAttach>(proc.state(), paramId, s);
}

void RhythmAudioProcessorEditor::pushProgressionText()
{
    proc.valueTree().setProperty(rhythm::params::progressionText,
                                 progressionEditor.getText(), nullptr);
    proc.invalidateSequence();
}

void RhythmAudioProcessorEditor::timerCallback()
{
    auto name = proc.getCurrentChordName();
    chordReadout.setText(name.isEmpty() ? juce::String("—") : name,
                         juce::dontSendNotification);
}

void RhythmAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient grad(juce::Colour(0xff21262e), 0, 0,
                              juce::Colour(0xff15181d), 0, (float)getHeight(), false);
    g.setGradientFill(grad);
    g.fillAll();
}

void RhythmAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced(12);

    auto top = r.removeFromTop(28);
    chordReadout.setBounds(top.removeFromRight(220));
    titleLabel.setBounds(top);

    r.removeFromTop(6);

    // Progression editor.
    progLabel.setBounds(r.removeFromTop(18));
    progressionEditor.setBounds(r.removeFromTop(70));
    r.removeFromTop(10);

    // Combo row.
    auto combos = r.removeFromTop(46);
    auto cw = combos.getWidth() / 3;
    auto layoutCombo = [](juce::Rectangle<int> area, juce::Label& l, juce::ComboBox& c) {
        l.setBounds(area.removeFromTop(16));
        c.setBounds(area.reduced(2, 0));
    };
    layoutCombo(combos.removeFromLeft(cw).reduced(3, 0), modeLabel, modeBox);
    layoutCombo(combos.removeFromLeft(cw).reduced(3, 0), voicingLabel, voicingBox);
    layoutCombo(combos.reduced(3, 0), patternLabel, patternBox);

    r.removeFromTop(10);

    // Toggle row.
    auto toggles = r.removeFromTop(26);
    voiceLeadingButton.setBounds(toggles.removeFromLeft(160));
    bassButton.setBounds(toggles.removeFromLeft(140));

    r.removeFromTop(8);

    // Knob grid (7 knobs).
    juce::Slider* knobs[] = { &octaveSlider, &maxNotesSlider, &swingSlider,
                              &velocitySlider, &transposeSlider, &channelSlider,
                              &liveBarsSlider };
    juce::Label* knobLabels[] = { &octaveLabel, &maxNotesLabel, &swingLabel,
                                  &velocityLabel, &transposeLabel, &channelLabel,
                                  &liveBarsLabel };
    const int n = (int)std::size(knobs);
    auto knobArea = r;
    int kw = knobArea.getWidth() / n;
    for (int i = 0; i < n; ++i) {
        auto cell = knobArea.removeFromLeft(kw);
        knobLabels[i]->setBounds(cell.removeFromTop(16));
        knobs[i]->setBounds(cell.reduced(2));
    }
}
