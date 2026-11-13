// Offline test of the actual plugin: renders every preset through processBlock and snapshots the editor.
#include "../../Source/PluginProcessor.h"
#include "../../Source/PluginEditor.h"
#include <random>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::String outDir = argc > 1 ? argv[1] : "/tmp";
    AnjiAmpProcessor proc;
    proc.setPlayConfigDetails (1, 2, 48000, 256);
    proc.prepareToPlay (48000, 256);

    std::mt19937 rng (3); std::uniform_real_distribution<float> U (-1, 1);
    const int N = 48000 * 2; std::vector<float> di ((size_t) N, 0);
    const float f[] = { 82.4f, 123.5f, 164.8f, 207.7f, 246.9f, 329.6f };
    for (int k = 0; k < 6; ++k) { int L = (int) (48000 / f[k]); std::vector<float> b ((size_t) L); for (auto& v : b) v = U (rng); int idx = 0;
        for (int n = k * 500; n < N; ++n) { float v = b[(size_t) idx]; int j = (idx + 1) % L; b[(size_t) idx] = 0.995f * 0.5f * (v + b[(size_t) j]); idx = j; di[(size_t) n] += v; } }
    float mx = 0; for (float v : di) mx = std::max (mx, std::abs (v)); for (auto& v : di) v *= 0.5f / mx;

    bool ok = true;
    for (int p = 0; p < proc.getNumPrograms(); ++p)
    {
        proc.setCurrentProgram (p);
        juce::AudioBuffer<float> buf (2, 256); juce::MidiBuffer midi; double s = 0; bool bad = false;
        for (int pos = 0; pos + 256 <= N; pos += 256)
        {
            buf.clear(); buf.copyFrom (0, 0, di.data() + pos, 256);
            proc.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i) { float v = buf.getSample (ch, i); if (! std::isfinite (v)) bad = true; s += v * v; }
        }
        const double db = 10 * std::log10 (s / (2.0 * N) + 1e-20);
        std::printf ("%-24s %6.1f dB %s\n", proc.getProgramName (p).toRawUTF8(), db, bad ? "NaN!" : "");
        ok = ok && ! bad;
    }

    // state round trip
    proc.setCurrentProgram (5);
    juce::MemoryBlock mb; proc.getStateInformation (mb);
    proc.setCurrentProgram (0); proc.setStateInformation (mb.getData(), (int) mb.getSize());
    std::printf ("state restore -> preset %d, amp=%g\n", proc.getCurrentProgram(), proc.apvts.getRawParameterValue ("amp")->load());

    for (int p : { 0, 13 })
    {
        proc.setCurrentProgram (p);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setVisible (true);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        juce::File f (outDir + "/ui_" + juce::String (p) + ".png"); f.deleteFile();
        juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("wrote %s\n", f.getFullPathName().toRawUTF8());
    }
    return ok ? 0 : 1;
}
