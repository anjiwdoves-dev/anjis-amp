#include "../Source/Presets.h"
#include <cstdio>
#include <random>
using namespace anji;
static const float FS = 48000;
std::vector<float> pluck (float f, int start, int N, std::vector<float>& out, float amp, std::mt19937& rng) {
    std::uniform_real_distribution<float> U (-1, 1); int L = (int) (FS / f); std::vector<float> b ((size_t) L);
    for (auto& v : b) v = U (rng); int idx = 0;
    for (int n = start; n < N; ++n) { float v = b[(size_t) idx]; int j = (idx + 1) % L; b[(size_t) idx] = 0.995f * 0.5f * (v + b[(size_t) j]); idx = j; out[(size_t) n] += v * amp; }
    return out;
}
int main() {
    // test riff: strummed chord, then picked single notes
    std::mt19937 rng (7); int N = (int) (FS * 4); std::vector<float> di ((size_t) N, 0);
    float chord[] = { 82.4f, 123.5f, 164.8f, 207.7f, 246.9f, 329.6f };
    for (int k = 0; k < 6; ++k) pluck (chord[k], k * 500, N, di, 1.0f, rng);
    float line[] = { 329.6f, 392.0f, 440.0f, 493.9f, 587.3f, 659.3f, 587.3f, 493.9f };
    for (int k = 0; k < 8; ++k) pluck (line[k], (int) (FS * (2.0f + k * 0.25f)), N, di, 2.2f, rng);
    float mx = 0; for (float v : di) mx = std::max (mx, std::fabs (v)); for (float& v : di) v *= 0.5f / mx;
    for (auto& d : factoryPresets()) {
        Params p = presetParams (d); 
        Engine e; e.prepare (FS); e.setParams (p);
        std::vector<float> L ((size_t) N), R ((size_t) N); e.process (di.data(), L.data(), R.data(), N);
        double s = 0; float pk = 0; for (int i = 0; i < N; ++i) { s += 0.5 * ((double) L[i] * L[i] + (double) R[i] * R[i]); pk = std::max (pk, std::max (std::fabs (L[i]), std::fabs (R[i]))); }
        double db = 10 * std::log10 (s / N + 1e-20);
        std::printf ("%s\t%.2f\t%.2f\n", d.name, db, 20 * std::log10 (pk + 1e-9));
    }
}
