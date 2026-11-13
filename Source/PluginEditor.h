#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace ui
{
    const juce::Colour bg     { 0xfff5f2ed };
    const juce::Colour card   { 0xffffffff };
    const juce::Colour border { 0xffe6e1d9 };
    const juce::Colour ink    { 0xff1f1d1a };
    const juce::Colour muted  { 0xff8a847a };
    const juce::Colour accent { 0xffe0705a };
    const juce::Colour track  { 0xffebe6de };

    inline juce::Font font (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    }
}

class AnjiLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AnjiLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return ui::font (13.0f, true); }
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return ui::font (14.0f); }
    juce::Font getPopupMenuFont() override { return ui::font (14.0f); }
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

// A rotary knob with its name above it
struct Knob : public juce::Component
{
    Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& name);
    void resized() override;
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// White rounded panel with a title and optional power switch
struct Card : public juce::Component
{
    Card (const juce::String& title, juce::AudioProcessorValueTreeState* s = nullptr, const juce::String& powerId = {});
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Rectangle<int> content() const { return getBounds().reduced (14).withTrimmedTop (24); }
    juce::String title;
    juce::ToggleButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
};

class AnjiAmpEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AnjiAmpEditor (AnjiAmpProcessor&);
    ~AnjiAmpEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    Knob* knob (const juce::String& id, const juce::String& name);
    void setAmp (int index);
    void stepPreset (int delta);
    static juce::File presetFolder();

    AnjiAmpProcessor& proc;
    AnjiLookAndFeel lnf;

    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "Save" }, loadBtn { "Load" };
    juce::Label notes;

    Card gateCard, compCard, driveCard, ampCard, cabCard, tremCard, chorusCard, delayCard, reverbCard;
    std::vector<std::unique_ptr<Knob>> knobs;

    juce::ComboBox driveType, cabType;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> driveTypeAtt, cabTypeAtt;

    juce::OwnedArray<juce::TextButton> ampButtons;
    juce::Label ampBlurb;
    juce::ToggleButton bright { "Bright" }, irToggle { "IR" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> brightAtt, irAtt;
    juce::TextButton irLoadBtn { "Load IR..." };
    juce::Label irName;

    Knob *gateK, *compAmt, *compLvl, *odDrive, *odTone, *odLevel;
    std::vector<Knob*> ampKnobs;
    Knob *micK, *tRate, *tDepth, *cRate, *cDepth, *cMix, *dTime, *dFb, *dTone, *dMix, *rSize, *rTone, *rMix, *inK, *outK;

    std::unique_ptr<juce::FileChooser> chooser;
    int shownPreset = -2, shownAmp = -1;
    float inLevel = 0, outLevel = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnjiAmpEditor)
};
