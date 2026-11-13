#include "../Source/Presets.h"
#include <cstdio>
#include <random>
using namespace anji;
int main() {
    const float FS = 48000; const int N = 48000 * 2;
    std::mt19937 rng (3); std::uniform_real_distribution<float> U (-1, 1); std::vector<float> di ((size_t) N, 0);
    const float f[] = { 82.4f, 123.5f, 164.8f, 207.7f, 246.9f, 329.6f };
    for (int k = 0; k < 6; ++k) { int L = (int) (FS / f[k]); std::vector<float> b ((size_t) L); for (auto& v : b) v = U (rng); int idx = 0;
        for (int n = k * 500; n < N; ++n) { float v = b[(size_t) idx]; int j = (idx + 1) % L; b[(size_t) idx] = 0.995f * 0.5f * (v + b[(size_t) j]); idx = j; di[(size_t) n] += v; } }
    float mx = 0; for (float v : di) mx = std::max (mx, std::fabs (v)); for (auto& v : di) v *= 0.5f / mx;
    FILE* fi = std::fopen ("/tmp/di.f32", "wb"); std::fwrite (di.data(), 4, (size_t) N, fi); std::fclose (fi);
    FILE* fo = std::fopen ("/tmp/cpp_out.f32", "wb");
    for (auto& d : factoryPresets()) { Engine e; e.prepare (FS); e.setParams (presetParams (d));
        std::vector<float> L ((size_t) N), R ((size_t) N);
        for (int pos = 0; pos < N; pos += 128) e.process (di.data() + pos, L.data() + pos, R.data() + pos, 128);
        std::fwrite (L.data(), 4, (size_t) N, fo); std::fwrite (R.data(), 4, (size_t) N, fo); }
    std::fclose (fo);
}
