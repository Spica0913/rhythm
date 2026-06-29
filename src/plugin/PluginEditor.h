#pragma once

#include "PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

// The plugin's GUI: chord-progression text entry, voicing/pattern selectors,
// performance sliders, and a live chord-name readout.
class RhythmAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
public:
    explicit RhythmAudioProcessorEditor(RhythmAudioProcessor&);
    ~RhythmAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void pushProgressionText();

    using APVTS = juce::AudioProcessorValueTreeState;
    using ComboAttach = APVTS::ComboBoxAttachment;
    using SliderAttach = APVTS::SliderAttachment;
    using ButtonAttach = APVTS::ButtonAttachment;

    // Helper to build a labelled rotary slider bound to a parameter.
    void addSlider(juce::Slider& s, juce::Label& l, const juce::String& text,
                   const char* paramId, std::unique_ptr<SliderAttach>& attach);
    void addCombo(juce::ComboBox& c, juce::Label& l, const juce::String& text,
                  const juce::StringArray& items, const char* paramId,
                  std::unique_ptr<ComboAttach>& attach);

    RhythmAudioProcessor& proc;

    juce::Label titleLabel;
    juce::Label chordReadout;

    juce::Label progLabel;
    juce::TextEditor progressionEditor;

    juce::ComboBox modeBox, voicingBox, patternBox;
    juce::Label modeLabel, voicingLabel, patternLabel;
    std::unique_ptr<ComboAttach> modeAttach, voicingAttach, patternAttach;

    juce::Slider octaveSlider, maxNotesSlider, swingSlider, velocitySlider,
                 transposeSlider, channelSlider, liveBarsSlider;
    juce::Label octaveLabel, maxNotesLabel, swingLabel, velocityLabel,
                transposeLabel, channelLabel, liveBarsLabel;
    std::unique_ptr<SliderAttach> octaveAttach, maxNotesAttach, swingAttach,
        velocityAttach, transposeAttach, channelAttach, liveBarsAttach;

    juce::ToggleButton voiceLeadingButton { "Voice leading" };
    juce::ToggleButton bassButton { "Bass note" };
    std::unique_ptr<ButtonAttach> voiceLeadingAttach, bassAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RhythmAudioProcessorEditor)
};
