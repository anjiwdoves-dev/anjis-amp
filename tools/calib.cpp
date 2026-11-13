#include "../Source/AnjiDSP.h"
#include <cstdio>
#include <random>
using namespace anji;
static const float FS = 48000;
std::vector<float> strum (const std::vector<float>& freqs, float seconds, float peak, unsigned seed = 1)
{
    std::mt19937 rng (seed); std::uniform_real_distribution<float> U (-1, 1);
    int N = (int) (FS * seconds); std::vector<float> out ((size_t) N, 0);
    for (size_t k = 0; k < freqs.size(); ++k) {
        int start = (int) (k * FS * 0.012f); int L = (int) (FS / freqs[k]);
        std::vector<float> b ((size_t) L); for (auto& v : b) v = U (rng); int idx = 0;
        for (int n = start; n < N; ++n) { float v = b[(size_t) idx]; int j = (idx + 1) % L;
            b[(size_t) idx] = 0.996f * 0.5f * (v + b[(size_t) j]); idx = j; out[(size_t) n] += v; } }
    float mx = 0; for (float v : out) mx = std::max (mx, std::fabs (v));
    for (float& v : out) v *= peak / mx; return out;
}
double rmsDb (Params p, const std::vector<float>& in) {
    Engine e; e.prepare (FS); e.setParams (p);
    std::vector<float> L (in.size()), R (in.size()); e.process (in.data(), L.data(), R.data(), (int) in.size());
    double s = 0; for (float x : L) { if (!std::isfinite(x)) return 999; s += (double) x * x; } return 10 * std::log10 (s / L.size() + 1e-20);
}
int main() {
    auto di = strum ({ 82.4f, 123.5f, 164.8f, 207.7f, 246.9f, 329.6f }, 2.0f, 0.5f);
    auto diq = di; for (auto& v : diq) v *= 0.1f;
    std::printf ("%-9s", "amp"); for (int g = 0; g <= 10; g += 2) std::printf ("   g%-2d cmp/lvl", g); std::printf ("\n");
    for (int a = 0; a < kNumAmps; ++a) {
        std::printf ("%-9s", kAmps[a].name);
        for (int g = 0; g <= 10; g += 2) { Params p; p.amp = a; p.gain = (float) g; p.cab = 3;
            double hi = rmsDb (p, di), lo = rmsDb (p, diq); std::printf ("  %5.1f/%6.1f", 20 - (hi - lo), hi); }
        std::printf ("\n"); }
}
