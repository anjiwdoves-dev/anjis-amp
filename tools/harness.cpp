// Test harness: stability, distortion and loudness per amp model.
#include "../Source/AnjiDSP.h"
#include <cstdio>
#include <random>
using namespace anji;

static const float FS = 48000;

// Karplus-Strong strum (DI-like), same algorithm as web demo
std::vector<float> strum (const std::vector<float>& freqs, float seconds, float peak, unsigned seed = 1)
{
    std::mt19937 rng (seed); std::uniform_real_distribution<float> U (-1, 1);
    int N = (int) (FS * seconds); std::vector<float> out ((size_t) N, 0);
    for (size_t k = 0; k < freqs.size(); ++k)
    {
        int start = (int) (k * FS * 0.012f); int L = (int) (FS / freqs[k]);
        std::vector<float> b ((size_t) L); for (auto& v : b) v = U (rng);
        float last = 0; int idx = 0;
        for (int n = start; n < N; ++n) { float v = b[(size_t) idx]; int j = (idx + 1) % L;
            float nv = 0.996f * 0.5f * (v + b[(size_t) j]); b[(size_t) idx] = nv; idx = j;
            float y = v - last * 0.0f; last = v; out[(size_t) n] += y; }
    }
    float mx = 0; for (float v : out) mx = std::max (mx, std::fabs (v));
    for (float& v : out) v *= peak / mx;
    return out;
}

double rms (const std::vector<float>& v) { double s = 0; for (float x : v) s += (double) x * x; return std::sqrt (s / (double) v.size()); }

int main (int argc, char** argv)
{
    auto di = strum ({ 82.4f, 123.5f, 164.8f, 207.7f, 246.9f, 329.6f }, 3.0f, 0.5f);
    // sine for THD
    std::vector<float> sine ((size_t) FS); for (size_t i = 0; i < sine.size(); ++i) sine[i] = 0.3f * std::sin (2 * kPi * 220 * i / FS);

    std::printf ("%-10s %6s %8s %8s %8s\n", "amp", "gain", "rmsDb", "thd%", "peak");
    for (int a = 0; a < kNumAmps; ++a)
        for (float g : { 0.0f, 3.0f, 5.0f, 7.0f, 10.0f })
        {
            Engine e; e.prepare (FS); Params p; p.amp = a; p.gain = g; p.cab = 3; e.setParams (p);
            std::vector<float> L (di.size()), R (di.size());
            e.process (di.data(), L.data(), R.data(), (int) di.size());
            float pk = 0; bool bad = false; for (float v : L) { if (! std::isfinite (v)) bad = true; pk = std::max (pk, std::fabs (v)); }
            double r = rms (L);

            Engine e2; e2.prepare (FS); p.cab = kCabOff; e2.setParams (p);
            std::vector<float> L2 (sine.size()), R2 (sine.size());
            e2.process (sine.data(), L2.data(), R2.data(), (int) sine.size());
            // THD via Goertzel on second half
            size_t s0 = sine.size() / 2; double re = 0, im = 0, tot = 0;
            for (size_t i = s0; i < sine.size(); ++i) { double ph = 2 * M_PI * 220 * i / FS; re += L2[i] * std::cos (ph); im += L2[i] * std::sin (ph); tot += (double) L2[i] * L2[i]; }
            double n = (double) (sine.size() - s0); double fund = 2 * (re * re + im * im) / (n * n); double totp = tot / n;
            double thd = std::sqrt (std::max (0.0, totp - fund) / std::max (fund, 1e-12)) * 100;
            std::printf ("%-10s %6.1f %8.1f %8.1f %8.3f %s\n", kAmps[a].name, g, 20 * std::log10 (r + 1e-12), thd, pk, bad ? "NAN!" : "");
        }
}
