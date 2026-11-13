#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

// =============================================================================
//  Look and feel
// =============================================================================
AnjiLookAndFeel::AnjiLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, ui::muted);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, ui::accent.withAlpha (0.25f));
    setColour (juce::Label::textColourId, ui::ink);
    setColour (juce::TextButton::textColourOffId, ui::ink);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::ComboBox::textColourId, ui::ink);
    setColour (juce::ComboBox::arrowColourId, ui::muted);
    setColour (juce::PopupMenu::backgroundColourId, ui::card);
    setColour (juce::PopupMenu::textColourId, ui::ink);
    setColour (juce::PopupMenu::headerTextColourId, ui::muted);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::accent.withAlpha (0.14f));
    setColour (juce::PopupMenu::highlightedTextColourId, ui::ink);
    setColour (juce::TextEditor::textColourId, ui::ink);
    setColour (juce::TextEditor::highlightColourId, ui::accent.withAlpha (0.25f));
    setColour (juce::CaretComponent::caretColourId, ui::accent);
    setColour (juce::ResizableWindow::backgroundColourId, ui::bg);
    setColour (juce::AlertWindow::backgroundColourId, ui::card);
    setColour (juce::AlertWindow::textColourId, ui::ink);
}

void AnjiLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float ang = a0 + pos * (a1 - a0);
    const float lw = juce::jmax (2.5f, r * 0.085f);
    const float ar = r - lw * 0.5f;

    juce::Path trackArc; trackArc.addCentredArc (c.x, c.y, ar, ar, 0, a0, a1, true);
    g.setColour (ui::track);
    g.strokePath (trackArc, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valArc; valArc.addCentredArc (c.x, c.y, ar, ar, 0, a0, ang, true);
    g.setColour (s.isEnabled() ? ui::accent : ui::muted);
    g.strokePath (valArc, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float br = r - lw * 2.4f;
    g.setColour (juce::Colours::black.withAlpha (0.06f));
    g.fillEllipse (c.x - br, c.y - br + 2.0f, br * 2, br * 2);
    g.setColour (ui::card);
    g.fillEllipse (c.x - br, c.y - br, br * 2, br * 2);
    g.setColour (ui::border);
    g.drawEllipse (c.x - br, c.y - br, br * 2, br * 2, 1.0f);

    const auto p1 = c.getPointOnCircumference (br * 0.30f, ang);
    const auto p2 = c.getPointOnCircumference (br * 0.78f, ang);
    g.setColour (ui::ink);
    g.drawLine ({ p1, p2 }, juce::jmax (1.8f, br * 0.09f));
}

void AnjiLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto bounds = b.getLocalBounds().toFloat();
    const float pw = 30.0f, ph = 16.0f;
    const auto pill = juce::Rectangle<float> (bounds.getRight() - pw - 1, bounds.getCentreY() - ph * 0.5f, pw, ph);
    const bool on = b.getToggleState();

    g.setColour (on ? ui::accent : (highlighted ? ui::border : ui::track));
    g.fillRoundedRectangle (pill, ph * 0.5f);
    const float d = ph - 4.0f;
    g.setColour (juce::Colours::white);
    g.fillEllipse (on ? pill.getRight() - d - 2 : pill.getX() + 2, pill.getY() + 2, d, d);

    if (b.getButtonText().isNotEmpty())
    {
        g.setColour (on ? ui::ink : ui::muted);
        g.setFont (ui::font (12.5f, true));
        g.drawText (b.getButtonText(), bounds.withRight (pill.getX() - 6), juce::Justification::centredRight);
    }
}

void AnjiLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const float rad = r.getHeight() * 0.5f;
    if (b.getToggleState())
    {
        g.setColour (ui::ink);
        g.fillRoundedRectangle (r, rad);
    }
    else
    {
        g.setColour (down ? ui::track : highlighted ? ui::bg : ui::card);
        g.fillRoundedRectangle (r, rad);
        g.setColour (ui::border);
        g.drawRoundedRectangle (r, rad, 1.0f);
    }
}

void AnjiLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (ui::card);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (box.hasKeyboardFocus (true) ? ui::muted : ui::border);
    g.drawRoundedRectangle (r, 8.0f, 1.0f);

    const float cx = (float) w - 16.0f, cy = (float) h * 0.5f;
    juce::Path chevron;
    chevron.startNewSubPath (cx - 4.0f, cy - 2.0f);
    chevron.lineTo (cx, cy + 2.0f);
    chevron.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (ui::muted);
    g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Label* AnjiLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (ui::font (11.5f));
    l->setColour (juce::Label::textColourId, ui::muted);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    return l;
}

// =============================================================================
//  Knob / Card
// =============================================================================
Knob::Knob (APVTS& s, const juce::String& id, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.22f, juce::MathConstants<float>::pi * 2.78f, true);
    slider.setMouseDragSensitivity (220);
    slider.setColour (juce::Slider::textBoxTextColourId, ui::muted);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (ui::font (12.0f, true));
    label.setColour (juce::Label::textColourId, ui::ink);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (slider);
    addAndMakeVisible (label);
    att = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
    if (auto* p = s.getParameter (id))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

Card::Card (const juce::String& t, APVTS* s, const juce::String& powerId) : title (t)
{
    setInterceptsMouseClicks (false, true);
    if (s != nullptr && powerId.isNotEmpty())
    {
        addAndMakeVisible (power);
        powerAtt = std::make_unique<APVTS::ButtonAttachment> (*s, powerId, power);
        power.onStateChange = [this] { repaint(); };
    }
}

void Card::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.03f));
    g.fillRoundedRectangle (r.translated (0, 2), 14.0f);
    g.setColour (ui::card);
    g.fillRoundedRectangle (r, 14.0f);
    g.setColour (ui::border);
    g.drawRoundedRectangle (r, 14.0f, 1.0f);

    const bool active = ! power.isVisible() || power.getToggleState();
    g.setColour (active ? ui::accent : ui::border);
    g.fillEllipse (16.0f, 17.0f, 6.0f, 6.0f);
    g.setColour (active ? ui::ink : ui::muted);
    g.setFont (ui::font (11.5f, true).withExtraKerningFactor (0.12f));
    g.drawText (title.toUpperCase(), 28, 10, getWidth() - 80, 20, juce::Justification::centredLeft);
}

void Card::resized()
{
    power.setBounds (getWidth() - 46, 10, 36, 20);
}

// =============================================================================
//  Editor
// =============================================================================
Knob* AnjiAmpEditor::knob (const juce::String& id, const juce::String& name)
{
    knobs.push_back (std::make_unique<Knob> (proc.apvts, id, name));
    addAndMakeVisible (*knobs.back());
    return knobs.back().get();
}

AnjiAmpEditor::AnjiAmpEditor (AnjiAmpProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      gateCard ("Gate", &p.apvts, "gate_on"), compCard ("Compressor", &p.apvts, "comp_on"),
      driveCard ("Drive", &p.apvts, "od_on"), ampCard ("Amp"), cabCard ("Cabinet"),
      tremCard ("Tremolo", &p.apvts, "trem_on"), chorusCard ("Chorus", &p.apvts, "chorus_on"),
      delayCard ("Delay", &p.apvts, "delay_on"), reverbCard ("Reverb", &p.apvts, "reverb_on")
{
    setLookAndFeel (&lnf);

    for (auto* c : { &gateCard, &compCard, &driveCard, &ampCard, &cabCard, &tremCard, &chorusCard, &delayCard, &reverbCard })
        addAndMakeVisible (*c);

    // ---- presets
    juce::String lastCat;
    const auto& presets = anji::factoryPresets();
    for (size_t i = 0; i < presets.size(); ++i)
    {
        if (lastCat != presets[i].category) { presetBox.addSectionHeading (presets[i].category); lastCat = presets[i].category; }
        presetBox.addItem (presets[i].name, (int) i + 1);
    }
    presetBox.setJustificationType (juce::Justification::centred);
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id > 0 && id - 1 != proc.currentPreset.load()) proc.loadPreset (id - 1);
    };
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (1); };
    saveBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Save preset", presetFolder().getChildFile ("My Tone.anjipreset"), "*.anjipreset");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f != juce::File()) proc.saveUserPreset (f.withFileExtension ("anjipreset"));
                              });
    };
    loadBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load preset", presetFolder(), "*.anjipreset");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile()) proc.loadUserPreset (f);
                              });
    };
    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &prevBtn, &nextBtn, &saveBtn, &loadBtn, &notes })
        addAndMakeVisible (c);
    notes.setJustificationType (juce::Justification::centred);
    notes.setFont (ui::font (13.0f));
    notes.setColour (juce::Label::textColourId, ui::muted);

    inK  = knob ("in_gain", "In");
    outK = knob ("out_gain", "Out");

    // ---- pedals
    gateK   = knob ("gate_thresh", "Threshold");
    compAmt = knob ("comp_amount", "Sustain");
    compLvl = knob ("comp_level", "Level");
    for (int i = 0; i < anji::kNumDriveModes; ++i) driveType.addItem (anji::kDriveModes[i], i + 1);
    addAndMakeVisible (driveType);
    driveTypeAtt = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, "od_mode", driveType);
    odDrive = knob ("od_drive", "Drive");
    odTone  = knob ("od_tone", "Tone");
    odLevel = knob ("od_level", "Level");

    // ---- amp
    for (int i = 0; i < anji::kNumAmps; ++i)
    {
        auto* b = ampButtons.add (new juce::TextButton (anji::kAmps[i].name));
        b->setClickingTogglesState (false);
        b->onClick = [this, i] { setAmp (i); };
        addAndMakeVisible (b);
    }
    ampBlurb.setJustificationType (juce::Justification::centredLeft);
    ampBlurb.setFont (ui::font (13.0f));
    ampBlurb.setColour (juce::Label::textColourId, ui::muted);
    addAndMakeVisible (ampBlurb);
    for (auto [id, nm] : std::initializer_list<std::pair<const char*, const char*>> { { "gain", "Gain" }, { "bass", "Bass" }, { "mid", "Middle" }, { "treble", "Treble" },
                           { "presence", "Presence" }, { "resonance", "Resonance" }, { "master", "Master" } })
        ampKnobs.push_back (knob (id, nm));
    addAndMakeVisible (bright);
    brightAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "bright", bright);

    // ---- cab
    for (int i = 0; i < anji::kNumCabs; ++i) cabType.addItem (anji::kCabs[i].name, i + 1);
    addAndMakeVisible (cabType);
    cabTypeAtt = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, "cab", cabType);
    micK = knob ("mic", "Mic");
    addAndMakeVisible (irToggle);
    irAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "ir_on", irToggle);
    addAndMakeVisible (irLoadBtn);
    irLoadBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load cabinet IR", juce::File(), "*.wav;*.aif;*.aiff");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile()) proc.loadIR (f);
                              });
    };
    irName.setFont (ui::font (11.5f));
    irName.setColour (juce::Label::textColourId, ui::muted);
    irName.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (irName);

    // ---- fx
    tRate  = knob ("trem_rate", "Rate");   tDepth = knob ("trem_depth", "Depth");
    cRate  = knob ("chorus_rate", "Rate"); cDepth = knob ("chorus_depth", "Depth"); cMix = knob ("chorus_mix", "Mix");
    dTime  = knob ("delay_time", "Time");  dFb = knob ("delay_fb", "Repeat"); dTone = knob ("delay_tone", "Tone"); dMix = knob ("delay_mix", "Mix");
    rSize  = knob ("reverb_size", "Size"); rTone = knob ("reverb_tone", "Tone"); rMix = knob ("reverb_mix", "Mix");

    setSize (1100, 664);
    sendLookAndFeelChange();   // make every child pick up the custom look
    startTimerHz (30);
    timerCallback();
}

AnjiAmpEditor::~AnjiAmpEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

juce::File AnjiAmpEditor::presetFolder()
{
    auto d = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Anjis Amp").getChildFile ("Presets");
    d.createDirectory();
    return d;
}

void AnjiAmpEditor::setAmp (int index)
{
    if (auto* prm = proc.apvts.getParameter ("amp"))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) index));
        prm->endChangeGesture();
    }
}

void AnjiAmpEditor::stepPreset (int delta)
{
    const int n = (int) anji::factoryPresets().size();
    const int cur = juce::jmax (0, proc.currentPreset.load());
    proc.loadPreset ((cur + delta + n) % n);
}

void AnjiAmpEditor::timerCallback()
{
   #if JUCE_WINDOWS
    // Use the software renderer: most robust across GPUs/drivers and hosts
    if (auto* peer = getPeer())
        if (peer->getCurrentRenderingEngine() != 0)
            peer->setCurrentRenderingEngine (0);
   #endif

    const int preset = proc.currentPreset.load();
    if (preset != shownPreset)
    {
        shownPreset = preset;
        const auto& v = anji::factoryPresets();
        if (juce::isPositiveAndBelow (preset, (int) v.size()))
        {
            presetBox.setSelectedId (preset + 1, juce::dontSendNotification);
            notes.setText (v[(size_t) preset].notes, juce::dontSendNotification);
        }
        else
        {
            presetBox.setText ("User preset", juce::dontSendNotification);
            notes.setText ({}, juce::dontSendNotification);
        }
    }

    const int amp = juce::roundToInt (proc.apvts.getRawParameterValue ("amp")->load());
    if (amp != shownAmp)
    {
        shownAmp = amp;
        for (int i = 0; i < ampButtons.size(); ++i) ampButtons[i]->setToggleState (i == amp, juce::dontSendNotification);
        if (juce::isPositiveAndBelow (amp, anji::kNumAmps)) ampBlurb.setText (anji::kAmps[amp].blurb, juce::dontSendNotification);
    }

    const auto ir = proc.getIRName();
    irName.setText (ir.isEmpty() ? "No IR loaded" : ir, juce::dontSendNotification);
    irToggle.setEnabled (ir.isNotEmpty());

    const bool cabOn = ! (ir.isNotEmpty() && irToggle.getToggleState());
    cabType.setEnabled (cabOn);
    micK->setEnabled (cabOn);

    inLevel  = proc.inMeter.load();
    outLevel = proc.outMeter.load();
    repaint (getWidth() - 260, 0, 260, 90);
}

void AnjiAmpEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::bg);

    // wordmark
    g.setColour (ui::accent);
    g.fillEllipse (24.0f, 31.0f, 12.0f, 12.0f);
    g.setColour (ui::ink);
    g.setFont (ui::font (26.0f, true).withExtraKerningFactor (-0.03f));
    g.drawText ("anji's amp", 44, 18, 220, 38, juce::Justification::centredLeft);

    // meters next to In / Out knobs
    auto meter = [&] (int x, float level)
    {
        const auto r = juce::Rectangle<float> ((float) x, 22.0f, 5.0f, 50.0f);
        g.setColour (ui::track);
        g.fillRoundedRectangle (r, 2.5f);
        const float db = juce::Decibels::gainToDecibels (level, -60.0f);
        const float frac = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        auto fill = r.withTrimmedTop (r.getHeight() * (1.0f - frac));
        g.setColour (db > -1.0f ? juce::Colour (0xffd64545) : ui::accent);
        g.fillRoundedRectangle (fill, 2.5f);
    };
    meter (inK->getRight() + 2, inLevel);
    meter (outK->getRight() + 2, outLevel);
}

void AnjiAmpEditor::resized()
{
    const int pad = 20, gap = 12;

    // header
    auto header = juce::Rectangle<int> (0, 14, getWidth(), 60);
    outK->setBounds (getWidth() - pad - 72, 10, 60, 72);
    inK->setBounds (outK->getX() - 72, 10, 60, 72);
    auto presetArea = juce::Rectangle<int> (0, 0, 440, 36).withCentre ({ getWidth() / 2 - 20, header.getCentreY() });
    prevBtn.setBounds (presetArea.removeFromLeft (36));
    nextBtn.setBounds (presetArea.removeFromRight (36));
    presetBox.setBounds (presetArea.reduced (8, 0));
    saveBtn.setBounds (nextBtn.getRight() + 16, presetArea.getY(), 58, 36);
    loadBtn.setBounds (saveBtn.getRight() + 6, presetArea.getY(), 58, 36);
    notes.setBounds (pad, 76, getWidth() - 2 * pad, 20);

    auto row = [] (juce::Rectangle<int> area, std::initializer_list<juce::Component*> items)
    {
        const int w = area.getWidth() / (int) items.size();
        for (auto* c : items) c->setBounds (area.removeFromLeft (w).reduced (2, 0));
    };

    // pedals row
    auto r1 = juce::Rectangle<int> (pad, 104, getWidth() - 2 * pad, 150);
    gateCard.setBounds (r1.removeFromLeft (150)); r1.removeFromLeft (gap);
    compCard.setBounds (r1.removeFromLeft (230)); r1.removeFromLeft (gap);
    driveCard.setBounds (r1);
    row (gateCard.content(), { gateK });
    row (compCard.content(), { compAmt, compLvl });
    {
        auto c = driveCard.content();
        auto left = c.removeFromLeft (150);
        driveType.setBounds (left.withSizeKeepingCentre (130, 32));
        row (c, { odDrive, odTone, odLevel });
    }

    // amp
    ampCard.setBounds (pad, 266, getWidth() - 2 * pad, 220);
    {
        auto c = ampCard.content();
        auto pills = c.removeFromTop (30);
        const int pw = pills.getWidth() / anji::kNumAmps;
        for (auto* b : ampButtons) b->setBounds (pills.removeFromLeft (pw).reduced (3, 0));
        c.removeFromTop (6);
        ampBlurb.setBounds (c.removeFromTop (20).reduced (4, 0));
        c.removeFromTop (4);
        bright.setBounds (c.removeFromRight (110).withSizeKeepingCentre (100, 24));
        const int kw = c.getWidth() / (int) ampKnobs.size();
        for (auto* k : ampKnobs) k->setBounds (c.removeFromLeft (kw).reduced (6, 0));
    }

    // cab + fx row
    auto r3 = juce::Rectangle<int> (pad, 498, getWidth() - 2 * pad, 150);
    cabCard.setBounds (r3.removeFromLeft (280)); r3.removeFromLeft (gap);
    tremCard.setBounds (r3.removeFromLeft (150)); r3.removeFromLeft (gap);
    chorusCard.setBounds (r3.removeFromLeft (190)); r3.removeFromLeft (gap);
    reverbCard.setBounds (r3.removeFromRight (190)); r3.removeFromRight (gap);
    delayCard.setBounds (r3);
    {
        auto c = cabCard.content();
        auto right = c.removeFromRight (76);
        micK->setBounds (right);
        cabType.setBounds (c.removeFromTop (30).reduced (2, 0));
        c.removeFromTop (8);
        auto irRow = c.removeFromTop (26);
        irLoadBtn.setBounds (irRow.removeFromLeft (92).reduced (2, 0));
        irToggle.setBounds (irRow.reduced (2, 0));
        irName.setBounds (c.removeFromTop (22));
    }
    row (tremCard.content(), { tRate, tDepth });
    row (chorusCard.content(), { cRate, cDepth, cMix });
    row (delayCard.content(), { dTime, dFb, dTone, dMix });
    row (reverbCard.content(), { rSize, rTone, rMix });
}
