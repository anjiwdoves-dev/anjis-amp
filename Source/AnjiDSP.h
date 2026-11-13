// =============================================================================
//  Anji's Amp — DSP core
//  Header-only, no framework dependencies. The JUCE plugin and the web version
//  (web/engine.js) implement exactly the same signal chain.
//
//  Chain:  in -> gate -> comp -> [2x oversampled: drive pedal -> preamp ->
//          tone stack -> power amp] -> cab -> tremolo -> chorus -> delay ->
//          reverb -> out
// =============================================================================
#pragma once
#include <cmath>
#include <vector>
#include <algorithm>
#include <cstring>

namespace anji
{
constexpr float kPi = 3.14159265358979323846f;

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g)  { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float clamp01 (float x)   { return std::min (1.0f, std::max (0.0f, x)); }

// Fast rational tanh (matches web engine exactly)
inline float ftanh (float x)
{
    if (x > 3.0f)  return 1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Triode-ish asymmetric stage: bias shifts the operating point -> even harmonics
inline float tube (float x, float bias) { return ftanh (x + bias) - ftanh (bias); }

// Solid-state style: flat until it hits the rail, then fairly hard
inline float solidClip (float x)
{
    const float x4 = x * x * x * x;
    return x / std::sqrt (std::sqrt (1.0f + x4));
}

// -----------------------------------------------------------------------------
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }

    inline float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setNorm (double B0, double B1, double B2, double A0, double A1, double A2)
    {
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }

    static double w0 (double fs, double f) { return 2.0 * 3.141592653589793 * std::min (f, fs * 0.49) / fs; }

    void lowpass (double fs, double f, double q)
    {
        const double w = w0 (fs, f), c = std::cos (w), al = std::sin (w) / (2 * q);
        setNorm ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void highpass (double fs, double f, double q)
    {
        const double w = w0 (fs, f), c = std::cos (w), al = std::sin (w) / (2 * q);
        setNorm ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void peak (double fs, double f, double q, double db)
    {
        const double A = std::pow (10.0, db / 40), w = w0 (fs, f), c = std::cos (w), al = std::sin (w) / (2 * q);
        setNorm (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowShelf (double fs, double f, double db)
    {
        const double A = std::pow (10.0, db / 40), w = w0 (fs, f), c = std::cos (w), s = std::sin (w);
        const double al = s / 2 * std::sqrt (2.0), sa = 2 * std::sqrt (A) * al;
        setNorm (A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
                 (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
    }
    void highShelf (double fs, double f, double db)
    {
        const double A = std::pow (10.0, db / 40), w = w0 (fs, f), c = std::cos (w), s = std::sin (w);
        const double al = s / 2 * std::sqrt (2.0), sa = 2 * std::sqrt (A) * al;
        setNorm (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
                 (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
    }
};

struct OnePole
{
    float a = 0, z = 0;
    void setLP (float fs, float f) { a = std::exp (-2.0f * kPi * std::min (f, fs * 0.49f) / fs); }
    inline float lp (float x) { z = x + a * (z - x); return z; }
    inline float hp (float x) { return x - lp (x); }
    void reset() { z = 0; }
};

// 2x oversampler, 6th-order Butterworth on both sides
struct Oversampler2x
{
    Biquad up[3], dn[3];
    void prepare (double fsBase)
    {
        const double q[3] = { 0.5176380902, 0.7071067812, 1.9318516526 };
        for (int i = 0; i < 3; ++i) { up[i].lowpass (fsBase * 2, fsBase * 0.45, q[i]); dn[i] = up[i]; up[i].reset(); dn[i].reset(); }
    }
    inline float u (float x) { for (auto& b : up) x = b.process (x); return x; }
    inline float d (float x) { for (auto& b : dn) x = b.process (x); return x; }
};

struct DelayLine
{
    std::vector<float> buf; int w = 0;
    void resize (int n) { buf.assign ((size_t) std::max (4, n), 0.0f); w = 0; }
    inline void push (float x) { buf[(size_t) w] = x; if (++w >= (int) buf.size()) w = 0; }
    // read 'd' samples behind the most recently written sample (fractional)
    inline float read (float d) const
    {
        const int n = (int) buf.size();
        d = std::min (std::max (d, 1.0f), (float) (n - 2));
        float rp = (float) w - 1.0f - d; while (rp < 0) rp += (float) n;
        int i0 = (int) rp; const float fr = rp - (float) i0; if (i0 >= n) i0 -= n; const int i1 = (i0 + 1) % n;
        return buf[(size_t) i0] + fr * (buf[(size_t) i1] - buf[(size_t) i0]);
    }
};

// -----------------------------------------------------------------------------
//  Model definitions
// -----------------------------------------------------------------------------
struct AmpDef
{
    const char* name;
    const char* blurb;
    float inHpf;
    float brightDb;            // bright switch shelf amount (fades as gain goes up)
    int   stages;
    float stGain[4], stBias[4], stLpf[4], stHpf[4];
    float gainMinDb, gainMaxDb;
    float bassF, bassRange, midF, midQ, midBase, midRange, trebF, trebRange;
    float powerDrive, sag, presF, resF;
    bool  solidState;
    float trimDb;              // output trim (calibrated so models sit at similar loudness)
};

static const AmpDef kAmps[] = {
 // name        blurb                                              inHpf bright st  stage gains            stage bias               stage LPF                     stage HPF            gmin gmax  bassF bR   midF  mQ   mBase mR   trebF tR   pDrv sag  pres  res  ss    trim
 { "Glass",    "American blackface clean. Huge headroom, scooped, sparkly.", 40, 6, 2, {1.0f,1.4f,0,0},  {0.15f,0.10f,0,0},  {12000,9000,0,0},          {30,40,0,0},        -10, 18, 100, 12, 500, 0.7f, -4, 6, 2200, 12, 0.8f, 0.15f, 4000, 90, false, 7.4f },
 { "Twinkle",  "Small combo clean that blooms when you dig in. Emo heaven.",  50, 5, 2, {1.6f,2.2f,0,0},  {0.20f,0.15f,0,0},  {10000,7000,0,0},          {40,60,0,0},         -6, 20, 110, 10, 600, 0.7f, -3, 5, 2500, 12, 1.2f, 0.40f, 4500, 95, false, -2.7f },
 { "Chime",    "British class-A top boost. Jangly highs, chewy breakup.",     60, 3, 2, {1.8f,2.0f,0,0},  {0.25f,0.20f,0,0},  {11000,8000,0,0},          {60,90,0,0},         -4, 22, 120, 10,1100, 0.9f,  2, 5, 3000, 12, 1.6f, 0.60f, 5000,100, false, -6.1f },
 { "Basement", "Big-iron British crunch. Loud, thick, punchy midrange.",      45, 3, 3, {1.4f,2.5f,1.5f,0},{0.20f,0.30f,0.10f,0},{9000,6500,7000,0},       {35,70,50,0},        -2, 26, 100, 10, 700, 0.6f,  1, 7, 2500, 10, 1.3f, 0.35f, 4000, 90, false, -8.0f },
 { "Plexi",    "'68 plexi, channels jumped. The classic rock sound.",         50, 4, 3, {1.5f,3.0f,1.6f,0},{0.25f,0.35f,0.15f,0},{10000,6000,7500,0},      {50,100,60,0},        0, 28, 100,  9, 650, 0.7f,  2, 6, 2800, 10, 1.8f, 0.50f, 4000, 90, false, -10.8f },
 { "800",      "Single-channel British master volume. Tight, cutting crunch.",80, 3, 3, {2.0f,4.0f,2.0f,0},{0.30f,0.40f,0.20f,0},{9000,5500,6500,0},       {90,180,80,0},       -4, 30, 110,  9, 750, 0.7f,  3, 6, 3000, 10, 1.5f, 0.30f, 4000, 95, false, -12.3f },
 { "Brown",    "Hot-rodded plexi. Saturated, singing 80s rock lead.",         70, 3, 4, {2.0f,3.5f,3.0f,1.5f},{0.30f,0.40f,0.30f,0.15f},{9000,6000,5000,7000},{80,150,120,60},  -6, 30, 100,  9, 700, 0.7f,  3, 6, 2800, 10, 1.8f, 0.45f, 4000, 90, false, -12.6f },
 { "Modern",   "High-gain American. Tight lows, compressed, articulate.",     90, 2, 4, {3.0f,5.0f,4.0f,2.0f},{0.30f,0.45f,0.35f,0.15f},{8000,5000,4500,6500},{150,250,200,80}, -8, 34, 120,  8, 600, 0.7f, -2, 7, 3200,  9, 1.2f, 0.20f, 4500,100, false, -11.9f },
 { "Jazz",     "Solid-state stereo clean. Flat, fast, never breaks up.",      30, 4, 1, {1.0f,0,0,0},      {0,0,0,0},           {15000,0,0,0},             {30,0,0,0},         -10, 16, 120, 12, 800, 0.7f,  0, 8, 3000, 12, 0.7f, 0.00f, 4000, 90, true, 9.8f },
};
constexpr int kNumAmps = (int) (sizeof (kAmps) / sizeof (kAmps[0]));

struct CabDef
{
    const char* name;
    float hpf, resF, resDb, p1F, p1Db, p1Q, p2F, p2Db, p2Q, lpf;
};

static const CabDef kCabs[] = {
    { "1x12 Cream",     80, 120, 2.0f,  700, -2.0f, 0.8f, 2500, 3.0f, 1.2f, 5500 },
    { "2x12 Jensen",    70, 100, 2.0f,  400, -2.0f, 0.8f, 2800, 3.0f, 1.2f, 6000 },
    { "2x12 Blue",      75, 110, 3.0f, 1200,  2.0f, 1.0f, 3500, 4.0f, 1.5f, 6500 },
    { "4x12 Greenback", 70, 100, 4.0f, 1500,  3.0f, 1.0f, 3000, 2.0f, 1.2f, 4800 },
    { "4x12 V30",       80, 110, 4.0f,  500, -2.0f, 0.9f, 2500, 5.0f, 2.0f, 5500 },
    { "Off (DI)",        0,   0, 0, 0, 0, 1, 0, 0, 1, 0 },
};
constexpr int kNumCabs = (int) (sizeof (kCabs) / sizeof (kCabs[0]));
constexpr int kCabOff  = kNumCabs - 1;

static const char* kDriveModes[] = { "Green", "Gold", "Treble", "Fuzz" };
constexpr int kNumDriveModes = 4;

// -----------------------------------------------------------------------------
//  Parameters (human units: knobs 0..10, dB, ms, Hz)
// -----------------------------------------------------------------------------
struct Params
{
    float inGain = 0, outGain = 0;                         // dB
    bool  gateOn = false;  float gateThresh = 3;           // 0..10
    bool  compOn = false;  float compAmount = 5, compLevel = 5;
    bool  odOn = false;    int odMode = 0; float odDrive = 5, odTone = 5, odLevel = 5;
    int   amp = 0;
    float gain = 5, bass = 5, mid = 5, treble = 5, presence = 5, resonance = 5, master = 5;
    bool  bright = false;
    int   cab = 0;         float mic = 3;                  // 0 = centre (bright) .. 10 = edge (dark)
    bool  tremOn = false;  float tremRate = 5, tremDepth = 5;      // Hz, 0..10
    bool  chorusOn = false; float chorusRate = 0.8f, chorusDepth = 5, chorusMix = 5;
    bool  delayOn = false; float delayTime = 380, delayFb = 3, delayMix = 2.5f, delayTone = 5;
    bool  reverbOn = false; float reverbSize = 5, reverbTone = 5, reverbMix = 2.5f;
};

// -----------------------------------------------------------------------------
//  Freeverb-style stereo reverb
// -----------------------------------------------------------------------------
struct Reverb
{
    struct Comb { std::vector<float> b; int i = 0; float store = 0;
        inline float process (float x, float fb, float damp) {
            float o = b[(size_t) i]; store = o * (1 - damp) + store * damp;
            b[(size_t) i] = x + store * fb; if (++i >= (int) b.size()) i = 0; return o; } };
    struct AP { std::vector<float> b; int i = 0;
        inline float process (float x) { float bo = b[(size_t) i]; float o = -x + bo;
            b[(size_t) i] = x + bo * 0.5f; if (++i >= (int) b.size()) i = 0; return o; } };

    Comb cl[8], cr[8]; AP al[4], ar[4]; DelayLine pre; float fs = 48000;

    void prepare (float sampleRate)
    {
        fs = sampleRate; const float s = fs / 44100.0f;
        const int c[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 }, a[4] = { 556, 441, 341, 225 };
        for (int k = 0; k < 8; ++k) { cl[k].b.assign ((size_t) (c[k] * s), 0); cr[k].b.assign ((size_t) ((c[k] + 23) * s), 0); cl[k].i = cr[k].i = 0; cl[k].store = cr[k].store = 0; }
        for (int k = 0; k < 4; ++k) { al[k].b.assign ((size_t) (a[k] * s), 0); ar[k].b.assign ((size_t) ((a[k] + 23) * s), 0); al[k].i = ar[k].i = 0; }
        pre.resize ((int) (fs * 0.05f));
    }
    inline void process (float in, float size01, float tone01, float& L, float& R)
    {
        pre.push (in);
        const float x = pre.read (fs * 0.018f) * 0.015f;
        const float fb = 0.70f + 0.28f * size01, damp = 0.05f + 0.6f * (1 - tone01);
        float l = 0, r = 0;
        for (int k = 0; k < 8; ++k) { l += cl[k].process (x, fb, damp); r += cr[k].process (x, fb, damp); }
        for (int k = 0; k < 4; ++k) { l = al[k].process (l); r = ar[k].process (r); }
        L = l * 3.0f; R = r * 3.0f;
    }
};

// -----------------------------------------------------------------------------
//  Engine
// -----------------------------------------------------------------------------
class Engine
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate; fsO = fs * 2;
        os.prepare (fs);
        delay.resize ((int) (fs * 2.1f));
        chorus.resize ((int) (fs * 0.05f));
        reverb.prepare (fs);
        reset();
        dirty = true;
    }

    void reset()
    {
        gateEnv = 0; gateGain = 1; gateHold = 0; compEnv = 0; sagEnv = 0;
        for (auto& b : pre) b.reset();
        for (auto& b : stLpf) b.reset();
        for (auto& b : stHpf) b.reset();
        for (auto& b : stack) b.reset();
        for (auto& b : pwr) b.reset();
        for (auto& b : cabF) b.reset();
        odHp.reset(); odHp2.reset(); odTone.reset(); odTone2.reset(); dlyLp.reset(); dlyHp.reset();
        tremPh = chorPh = 0; curDelay = -1;
        std::fill (delay.buf.begin(), delay.buf.end(), 0.0f);
        std::fill (chorus.buf.begin(), chorus.buf.end(), 0.0f);
        reverb.prepare (fs);
    }

    void setParams (const Params& np)
    {
        if (std::memcmp (&np, &p, sizeof (Params)) != 0) { p = np; dirty = true; }
    }

    // When true the built-in cab is bypassed (host applies an IR between pre and post)
    void setExternalCab (bool b) { if (b != externalCab) { externalCab = b; dirty = true; } }

    // mono in -> mono amp/cab out
    void processPre (const float* in, float* out, int n)
    {
        if (dirty) update();
        const AmpDef& a = kAmps[p.amp];
        for (int i = 0; i < n; ++i)
        {
            float x = in[i] * inG;

            // ---- gate
            if (p.gateOn)
            {
                const float ax = std::fabs (x);
                gateEnv = ax > gateEnv ? ax : gateEnv * gateRel;
                if (gateEnv > gateOpenT) gateHold = (int) (fs * 0.05f);
                else if (gateHold > 0) --gateHold;
                const float target = (gateEnv > gateOpenT || gateHold > 0) ? 1.0f : (gateEnv < gateCloseT ? 0.0f : gateGain);
                gateGain += (target - gateGain) * (target > gateGain ? 0.05f : 0.0015f);
                x *= gateGain;
            }

            // ---- compressor
            if (p.compOn)
            {
                const float ax = std::fabs (x);
                compEnv = ax > compEnv ? compEnv + (ax - compEnv) * compAtt : compEnv + (ax - compEnv) * compRelC;
                const float envDb = gainToDb (compEnv);
                const float grDb = envDb > compTh ? (compTh + (envDb - compTh) / compRatio) - envDb : 0.0f;
                x *= dbToGain (grDb) * compMakeup;
            }

            // ---- oversampled nonlinear section
            const float u0 = os.u (x * 2.0f), u1 = os.u (0.0f);
            const float y0 = nonlinear (u0, a), y1 = nonlinear (u1, a);
            os.d (y0); float y = os.d (y1);

            // ---- cab
            if (useCab) for (int k = 0; k < 6; ++k) y = cabF[k].process (y);

            out[i] = y * ampTrim;
        }
    }

    // mono amp out -> stereo FX out
    void processPost (const float* in, float* L, float* R, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            float x = in[i];

            if (p.tremOn)
            {
                tremPh += tremInc; if (tremPh > 1) tremPh -= 1;
                const float s = 0.5f + 0.5f * std::sin (2 * kPi * tremPh);
                x *= 1.0f - tremDepth * s * s * (3 - 2 * s);   // smooth "optical" shape
            }

            float l = x, r = x;

            if (p.chorusOn)
            {
                chorus.push (x);
                chorPh += chorInc; if (chorPh > 1) chorPh -= 1;
                const float base = fs * 0.007f, dep = fs * 0.0035f * chorDepth;
                const float wl = chorus.read (base + dep * std::sin (2 * kPi * chorPh));
                const float wr = chorus.read (base + dep * std::sin (2 * kPi * chorPh + kPi * 0.5f));
                l = x * (1 - chorMix * 0.5f) + wl * chorMix * 0.7f;
                r = x * (1 - chorMix * 0.5f) + wr * chorMix * 0.7f;
            }

            if (p.delayOn)
            {
                if (curDelay < 0) curDelay = dlyTarget;
                curDelay += (dlyTarget - curDelay) * 0.0002f;            // tape-ish glide on time changes
                const float d1 = delay.read (curDelay);
                const float d2 = delay.read (curDelay + fs * 0.011f);     // slight stereo smear
                float fbS = dlyHp.hp (dlyLp.lp (d1)) * dlyFb;
                fbS = ftanh (fbS * 1.2f) / 1.2f;                          // soft analog saturation in loop
                delay.push ((l + r) * 0.5f + fbS);
                l += d1 * dlyMix; r += d2 * dlyMix;
            }

            if (p.reverbOn)
            {
                float wl, wr; reverb.process ((l + r) * 0.5f, revSize, revTone, wl, wr);
                l = l * (1 - revMix * 0.3f) + wl * revMix;
                r = r * (1 - revMix * 0.3f) + wr * revMix;
            }

            L[i] = l * outG; R[i] = r * outG;
        }
    }

    // Convenience: full chain using built-in cab
    void process (const float* in, float* L, float* R, int n)
    {
        if ((int) tmp.size() < n) tmp.resize ((size_t) n);
        processPre (in, tmp.data(), n);
        processPost (tmp.data(), L, R, n);
    }

    const Params& params() const { return p; }

private:
    inline float driveStage (float x)
    {
        switch (p.odMode)
        {
            case 0: { // Green: mid-hump screamer
                const float c = odHp.hp (x);
                float y = x + ftanh (c * odGain) * 0.6f;
                y = odTone.lp (y); return y * odOut;
            }
            case 1: { // Gold: transparent, clean blend + hard-ish clip
                const float c = odHp.hp (x);
                float v = c * odGain; v = v / (1 + std::fabs (v));
                float y = x * 0.8f + v * 0.5f;
                y = y + (y - odTone.lp (y)) * (odToneAmt - 0.5f) * 1.5f;  // treble tilt
                return y * odOut;
            }
            case 2: { // Treble booster (germanium-style, asymmetric)
                const float c = x * 0.4f + odHp.hp (x) * odGain;
                float y = tube (c * 1.5f, 0.25f);
                y = odTone.lp (y); return y * odOut;
            }
            default: { // Fuzz: two hard stages + scooped tone
                float v = odHp.hp (x) * odGain; v = ftanh (v);
                v = odHp2.hp (v); v = ftanh (v * 6.0f + 0.1f) - 0.0997f;
                const float lo = odTone.lp (v), hi = v - odTone2.lp (v);
                return (lo * (1 - odToneAmt) + hi * odToneAmt * 1.6f) * odOut;
            }
        }
    }

    inline float nonlinear (float x, const AmpDef& a)
    {
        if (p.odOn) x = driveStage (x);

        x = pre[0].process (x) * preG;          // input HPF
        x = pre[1].process (x);                 // bright shelf
        for (int s = 0; s < a.stages; ++s)
        {
            x *= a.stGain[s];
            x = a.solidState ? solidClip (x) : tube (x, a.stBias[s]);
            x = stHpf[s].process (x);
            x = stLpf[s].process (x);
        }
        for (int k = 0; k < 3; ++k) x = stack[k].process (x);

        // power amp with sag
        const float ax = std::fabs (x);
        sagEnv = ax > sagEnv ? sagEnv + (ax - sagEnv) * sagAtt : sagEnv + (ax - sagEnv) * sagRel;
        float y = x * pwrDrive / (1.0f + a.sag * sagEnv * 2.0f);
        y = pwr[0].process (y); y = pwr[1].process (y);
        y = a.solidState ? solidClip (y) : tube (y, 0.06f);
        return y;
    }

    void update()
    {
        dirty = false;
        p.amp = std::min (std::max (p.amp, 0), kNumAmps - 1);
        p.cab = std::min (std::max (p.cab, 0), kNumCabs - 1);
        p.odMode = std::min (std::max (p.odMode, 0), kNumDriveModes - 1);
        const AmpDef& a = kAmps[p.amp];
        const float g01 = clamp01 (p.gain / 10);

        inG  = dbToGain (p.inGain);
        outG = dbToGain (p.outGain);

        gateOpenT  = dbToGain (-80.0f + 5.0f * p.gateThresh);
        gateCloseT = gateOpenT * 0.6f;
        gateRel    = std::exp (-1.0f / (fs * 0.03f));

        compTh = -8.0f - 3.2f * p.compAmount;  compRatio = 3.0f + 0.5f * p.compAmount;
        compMakeup = dbToGain (-compTh * (1 - 1 / compRatio) * 0.55f + (p.compLevel - 5) * 1.2f);
        compAtt = 1 - std::exp (-1.0f / (fs * 0.004f)); compRelC = 1 - std::exp (-1.0f / (fs * 0.15f));

        // drive pedal
        static const float driveRange[4][2] = { { 6, 36 }, { 0, 30 }, { 0, 18 }, { 18, 44 } };
        odGain = dbToGain (driveRange[p.odMode][0] + (driveRange[p.odMode][1] - driveRange[p.odMode][0]) * p.odDrive / 10);
        odOut  = dbToGain ((p.odLevel - 5) * 2.4f);
        odToneAmt = clamp01 (p.odTone / 10);
        switch (p.odMode)
        {
            case 0: odHp.setLP (fsO, 720); odTone.setLP (fsO, 700 * std::pow (7.0f, odToneAmt)); break;
            case 1: odHp.setLP (fsO, 300); odTone.setLP (fsO, 1500); break;
            case 2: odHp.setLP (fsO, 1200); odTone.setLP (fsO, 3000 + 9000 * odToneAmt); break;
            default: odHp.setLP (fsO, 120); odHp2.setLP (fsO, 150); odTone.setLP (fsO, 900); odTone2.setLP (fsO, 1100); break;
        }

        // preamp
        preG = dbToGain (a.gainMinDb + (a.gainMaxDb - a.gainMinDb) * g01);
        pre[0].highpass (fsO, a.inHpf, 0.7);
        pre[1].highShelf (fsO, 2500, p.bright ? a.brightDb * (1 - 0.7f * g01) : 0.0);
        for (int s = 0; s < a.stages; ++s)
        {
            stLpf[s].lowpass (fsO, a.stLpf[s], 0.6);
            stHpf[s].highpass (fsO, a.stHpf[s], 0.6);
        }
        stack[0].lowShelf (fsO, a.bassF, (p.bass - 5) / 5 * a.bassRange);
        stack[1].peak (fsO, a.midF, a.midQ, a.midBase + (p.mid - 5) / 5 * a.midRange);
        stack[2].highShelf (fsO, a.trebF, (p.treble - 5) / 5 * a.trebRange);

        pwrDrive = dbToGain (-20.0f + 3.2f * p.master) * a.powerDrive;
        pwr[0].highShelf (fsO, a.presF, (p.presence - 5) * 1.6f);
        pwr[1].peak (fsO, a.resF, 0.8, (p.resonance - 5) * 1.4f);
        sagAtt = 1 - std::exp (-1.0f / (fsO * 0.004f)); sagRel = 1 - std::exp (-1.0f / (fsO * 0.12f));
        ampTrim = dbToGain (a.trimDb);

        // cab
        useCab = ! externalCab && p.cab != kCabOff;
        if (useCab)
        {
            const CabDef& c = kCabs[p.cab];
            const float m = clamp01 (p.mic / 10);           // 0 = centre (bright), 1 = edge (dark)
            cabF[0].highpass (fs, c.hpf, 0.8);
            cabF[1].peak (fs, c.resF, 1.4, c.resDb);
            cabF[2].peak (fs, c.p1F, c.p1Q, c.p1Db);
            cabF[3].peak (fs, c.p2F, c.p2Q, c.p2Db + 3.0f * (1 - m) - 2.0f * m);
            cabF[4].lowpass (fs, c.lpf * (1.35f - 0.6f * m), 0.75);
            cabF[5].lowpass (fs, c.lpf * (1.6f - 0.6f * m), 1.0);
        }

        tremInc  = std::max (0.1f, p.tremRate) / fs; tremDepth = clamp01 (p.tremDepth / 10) * 0.9f;
        chorInc  = std::max (0.05f, p.chorusRate) / fs; chorDepth = clamp01 (p.chorusDepth / 10); chorMix = clamp01 (p.chorusMix / 10);
        dlyTarget = std::min (std::max (p.delayTime, 20.0f), 2000.0f) * 0.001f * fs;
        dlyFb = clamp01 (p.delayFb / 10) * 0.92f; dlyMix = clamp01 (p.delayMix / 10) * 0.9f;
        dlyLp.setLP (fs, 1500 + 8000 * clamp01 (p.delayTone / 10)); dlyHp.setLP (fs, 90);
        revSize = clamp01 (p.reverbSize / 10); revTone = clamp01 (p.reverbTone / 10); revMix = clamp01 (p.reverbMix / 10);
    }

    Params p; bool dirty = true, externalCab = false, useCab = true;
    float fs = 48000, fsO = 96000;
    Oversampler2x os;

    float inG = 1, outG = 1, ampTrim = 1;
    float gateEnv = 0, gateGain = 1, gateOpenT = 0, gateCloseT = 0, gateRel = 0; int gateHold = 0;
    float compEnv = 0, compTh = -20, compRatio = 4, compMakeup = 1, compAtt = 0, compRelC = 0;
    float odGain = 1, odOut = 1, odToneAmt = 0.5f; OnePole odHp, odHp2, odTone, odTone2;
    float preG = 1, pwrDrive = 1, sagEnv = 0, sagAtt = 0, sagRel = 0;
    Biquad pre[2], stLpf[4], stHpf[4], stack[3], pwr[2], cabF[6];
    float tremPh = 0, tremInc = 0, tremDepth = 0;
    DelayLine chorus; float chorPh = 0, chorInc = 0, chorDepth = 0, chorMix = 0;
    DelayLine delay; float curDelay = -1, dlyTarget = 0, dlyFb = 0, dlyMix = 0; OnePole dlyLp, dlyHp;
    Reverb reverb; float revSize = 0.5f, revTone = 0.5f, revMix = 0;
    std::vector<float> tmp;
};

} // namespace anji
