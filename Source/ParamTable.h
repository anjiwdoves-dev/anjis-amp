// Single source of truth for parameter IDs, ranges and defaults.
#pragma once
#include "AnjiDSP.h"

namespace anji
{
enum ParamType { kFloat, kBool, kChoice };

struct ParamInfo
{
    const char* id; const char* name; float min, max, def; ParamType type; const char* unit; float centre;
    void (*set) (Params&, float);
};

#define PF(id, nm, mn, mx, df, unit, ctr, field) { id, nm, mn, mx, df, kFloat,  unit, ctr, [] (Params& p, float v) { p.field = v; } }
#define PB(id, nm, df, field)                    { id, nm, 0, 1, df,   kBool,   "", 0,    [] (Params& p, float v) { p.field = v > 0.5f; } }
#define PC(id, nm, n, df, field)                 { id, nm, 0, (float) (n - 1), df, kChoice, "", 0, [] (Params& p, float v) { p.field = (int) std::lround (v); } }

static const ParamInfo kParams[] = {
    PF ("in_gain",      "Input",          -12, 12, 0,  "dB", 0, inGain),
    PF ("out_gain",     "Output",         -24, 12, 0,  "dB", 0, outGain),
    PB ("gate_on",      "Gate On",         0, gateOn),
    PF ("gate_thresh",  "Gate Threshold",  0, 10, 3,   "", 0, gateThresh),
    PB ("comp_on",      "Comp On",         0, compOn),
    PF ("comp_amount",  "Comp Sustain",    0, 10, 5,   "", 0, compAmount),
    PF ("comp_level",   "Comp Level",      0, 10, 5,   "", 0, compLevel),
    PB ("od_on",        "Drive On",        0, odOn),
    PC ("od_mode",      "Drive Type",      kNumDriveModes, 0, odMode),
    PF ("od_drive",     "Drive",           0, 10, 5,   "", 0, odDrive),
    PF ("od_tone",      "Drive Tone",      0, 10, 5,   "", 0, odTone),
    PF ("od_level",     "Drive Level",     0, 10, 5,   "", 0, odLevel),
    PC ("amp",          "Amp",             kNumAmps, 0, amp),
    PF ("gain",         "Gain",            0, 10, 5,   "", 0, gain),
    PF ("bass",         "Bass",            0, 10, 5,   "", 0, bass),
    PF ("mid",          "Mid",             0, 10, 5,   "", 0, mid),
    PF ("treble",       "Treble",          0, 10, 5,   "", 0, treble),
    PF ("presence",     "Presence",        0, 10, 5,   "", 0, presence),
    PF ("resonance",    "Resonance",       0, 10, 5,   "", 0, resonance),
    PF ("master",       "Master",          0, 10, 5,   "", 0, master),
    PB ("bright",       "Bright",          0, bright),
    PC ("cab",          "Cab",             kNumCabs, 0, cab),
    PF ("mic",          "Mic Position",    0, 10, 3,   "", 0, mic),
    PB ("trem_on",      "Trem On",         0, tremOn),
    PF ("trem_rate",    "Trem Rate",       1, 12, 5,   "Hz", 4, tremRate),
    PF ("trem_depth",   "Trem Depth",      0, 10, 5,   "", 0, tremDepth),
    PB ("chorus_on",    "Chorus On",       0, chorusOn),
    PF ("chorus_rate",  "Chorus Rate",     0.1f, 6, 0.8f, "Hz", 1, chorusRate),
    PF ("chorus_depth", "Chorus Depth",    0, 10, 5,   "", 0, chorusDepth),
    PF ("chorus_mix",   "Chorus Mix",      0, 10, 5,   "", 0, chorusMix),
    PB ("delay_on",     "Delay On",        0, delayOn),
    PF ("delay_time",   "Delay Time",      20, 1500, 380, "ms", 350, delayTime),
    PF ("delay_fb",     "Delay Feedback",  0, 10, 3,   "", 0, delayFb),
    PF ("delay_mix",    "Delay Mix",       0, 10, 2.5f, "", 0, delayMix),
    PF ("delay_tone",   "Delay Tone",      0, 10, 5,   "", 0, delayTone),
    PB ("reverb_on",    "Reverb On",       0, reverbOn),
    PF ("reverb_size",  "Reverb Size",     0, 10, 5,   "", 0, reverbSize),
    PF ("reverb_tone",  "Reverb Tone",     0, 10, 5,   "", 0, reverbTone),
    PF ("reverb_mix",   "Reverb Mix",      0, 10, 2.5f, "", 0, reverbMix),
};
#undef PF
#undef PB
#undef PC
constexpr int kNumParams = (int) (sizeof (kParams) / sizeof (kParams[0]));

inline const ParamInfo* findParam (const char* id)
{
    for (auto& p : kParams) if (std::strcmp (p.id, id) == 0) return &p;
    return nullptr;
}

inline Params defaultParams()
{
    Params p; for (auto& i : kParams) i.set (p, i.def); return p;
}
} // namespace anji
