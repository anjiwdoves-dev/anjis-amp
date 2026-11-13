#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace anji;

static juce::StringArray namesOf (const char* const* list, int n)
{
    juce::StringArray s; for (int i = 0; i < n; ++i) s.add (list[i]); return s;
}

juce::AudioProcessorValueTreeState::ParameterLayout AnjiAmpProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    juce::StringArray ampNames, cabNames;
    for (auto& a : kAmps) ampNames.add (a.name);
    for (auto& c : kCabs) cabNames.add (c.name);
    const auto driveNames = namesOf (kDriveModes, kNumDriveModes);

    for (auto& info : kParams)
    {
        const juce::ParameterID pid { info.id, 1 };
        if (info.type == kBool)
        {
            layout.add (std::make_unique<juce::AudioParameterBool> (pid, info.name, info.def > 0.5f));
        }
        else if (info.type == kChoice)
        {
            const juce::String id (info.id);
            const auto& names = id == "amp" ? ampNames : id == "cab" ? cabNames : driveNames;
            layout.add (std::make_unique<juce::AudioParameterChoice> (pid, info.name, names, (int) info.def));
        }
        else
        {
            juce::NormalisableRange<float> range (info.min, info.max);
            if (info.centre > 0) range.setSkewForCentre (info.centre);
            const juce::String unit (info.unit);
            auto attrs = juce::AudioParameterFloatAttributes()
                .withLabel (unit)
                .withStringFromValueFunction ([unit] (float v, int)
                {
                    if (unit == "dB") return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB";
                    if (unit == "ms") return juce::String (juce::roundToInt (v)) + " ms";
                    if (unit == "Hz") return juce::String (v, 2) + " Hz";
                    return juce::String (v, 1);
                });
            layout.add (std::make_unique<juce::AudioParameterFloat> (pid, info.name, range, info.def, attrs));
        }
    }
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "ir_on", 1 }, "IR Loader On", false));
    return layout;
}

AnjiAmpProcessor::AnjiAmpProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::mono(),   true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ANJI_STATE", createLayout())
{
    for (int i = 0; i < kNumParams; ++i) raw[(size_t) i] = apvts.getRawParameterValue (kParams[i].id);
    irOn = apvts.getRawParameterValue ("ir_on");
    loadPreset (0);
}

bool AnjiAmpProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    const bool inOk  = in  == juce::AudioChannelSet::mono() || in  == juce::AudioChannelSet::stereo();
    const bool outOk = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    return inOk && outOk;
}

void AnjiAmpProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    mono.setSize (2, juce::jmax (1, samplesPerBlock), false, true, true);
    convolution.prepare ({ sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock), 1 });
    convolution.reset();
}

void AnjiAmpProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0) return;
    if (mono.getNumSamples() < n) mono.setSize (2, n, false, true, true);

    Params p;
    for (int i = 0; i < kNumParams; ++i) kParams[i].set (p, raw[(size_t) i]->load());

    const bool useIR = irLoaded.load() && irOn->load() > 0.5f;
    engine.setExternalCab (useIR);
    engine.setParams (p);

    // Guitar is mono: use the first input channel
    inMeter.store (juce::jmax (buffer.getMagnitude (0, 0, n), inMeter.load() * 0.85f));

    float* m = mono.getWritePointer (0);
    engine.processPre (buffer.getReadPointer (0), m, n);

    if (useIR)
    {
        // Keep loudness roughly constant whatever IR is loaded
        float pin = 0; for (int i = 0; i < n; ++i) pin += m[i] * m[i];
        float* chans[] = { m };
        juce::dsp::AudioBlock<float> block (chans, 1, (size_t) n);
        convolution.process (juce::dsp::ProcessContextReplacing<float> (block));
        float pout = 0; for (int i = 0; i < n; ++i) pout += m[i] * m[i];
        irAccIn = irAccIn * 0.97f + pin; irAccOut = irAccOut * 0.97f + pout;
        const float target = juce::jlimit (0.06f, 16.0f, std::sqrt (irAccIn / juce::jmax (irAccOut, 1.0e-9f)));
        for (int i = 0; i < n; ++i) { irGain += (target - irGain) * 0.0005f; m[i] *= irGain; }
    }

    if (buffer.getNumChannels() >= 2)
    {
        engine.processPost (m, buffer.getWritePointer (0), buffer.getWritePointer (1), n);
    }
    else
    {
        float* tmpR = mono.getWritePointer (1);
        float* L = buffer.getWritePointer (0);
        engine.processPost (m, L, tmpR, n);
        for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + tmpR[i]);
    }

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);
    outMeter.store (juce::jmax (buffer.getMagnitude (0, 0, n), outMeter.load() * 0.85f));
}

const juce::String AnjiAmpProcessor::getProgramName (int index)
{
    const auto& v = factoryPresets();
    return juce::isPositiveAndBelow (index, (int) v.size()) ? juce::String (v[(size_t) index].name) : juce::String();
}

void AnjiAmpProcessor::loadPreset (int index)
{
    const auto& v = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) v.size())) return;
    const auto& d = v[(size_t) index];

    for (auto& info : kParams)
    {
        float value = info.def;
        for (auto& kv : d.values) if (std::strcmp (kv.first, info.id) == 0) value = kv.second;
        if (auto* prm = apvts.getParameter (info.id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
    }
    currentPreset.store (index);
}

bool AnjiAmpProcessor::loadIR (const juce::File& file)
{
    if (! file.existsAsFile()) return false;
    convolution.loadImpulseResponse (file, juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::yes, 0,
                                     juce::dsp::Convolution::Normalise::yes);
    irFile = file;
    irLoaded.store (true);
    if (auto* prm = apvts.getParameter ("ir_on")) prm->setValueNotifyingHost (1.0f);
    return true;
}

void AnjiAmpProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("irPath", irFile.getFullPathName(), nullptr);
    state.setProperty ("preset", currentPreset.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void AnjiAmpProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (! xml->hasTagName (apvts.state.getType())) return;
        auto state = juce::ValueTree::fromXml (*xml);
        const juce::String ir = state.getProperty ("irPath", "");
        currentPreset.store ((int) state.getProperty ("preset", 0));
        apvts.replaceState (state);
        if (ir.isNotEmpty()) { juce::File f (ir); if (f.existsAsFile()) { const bool on = irOn->load() > 0.5f; loadIR (f);
            if (auto* prm = apvts.getParameter ("ir_on")) prm->setValueNotifyingHost (on ? 1.0f : 0.0f); } }
    }
}

bool AnjiAmpProcessor::saveUserPreset (const juce::File& file)
{
    auto state = apvts.copyState();
    state.setProperty ("irPath", irFile.getFullPathName(), nullptr);
    if (auto xml = state.createXml()) return xml->writeTo (file);
    return false;
}

bool AnjiAmpProcessor::loadUserPreset (const juce::File& file)
{
    if (auto xml = juce::XmlDocument::parse (file))
    {
        if (! xml->hasTagName (apvts.state.getType())) return false;
        auto state = juce::ValueTree::fromXml (*xml);
        const juce::String ir = state.getProperty ("irPath", "");
        apvts.replaceState (state);
        if (ir.isNotEmpty()) { juce::File f (ir); if (f.existsAsFile()) loadIR (f); }
        currentPreset.store (-1);
        return true;
    }
    return false;
}

juce::AudioProcessorEditor* AnjiAmpProcessor::createEditor() { return new AnjiAmpEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AnjiAmpProcessor(); }
