// Anji's Amp — JS port of Source/AnjiDSP.h (same chain, same constants).
// Requires models.js (ANJI_AMPS, ANJI_CABS, ANJI_PARAMS).
(function (global) {
const PI = Math.PI;
const dbToGain = db => Math.pow(10, db * 0.05);
const gainToDb = g => 20 * Math.log10(Math.max(g, 1e-9));
const clamp01 = x => Math.min(1, Math.max(0, x));
function ftanh(x) { if (x > 3) return 1; if (x < -3) return -1; const x2 = x * x; return x * (27 + x2) / (27 + 9 * x2); }
const tube = (x, b) => ftanh(x + b) - ftanh(b);
function solidClip(x) { const x4 = x * x * x * x; return x / Math.sqrt(Math.sqrt(1 + x4)); }

class Biquad {
  constructor() { this.b0 = 1; this.b1 = 0; this.b2 = 0; this.a1 = 0; this.a2 = 0; this.z1 = 0; this.z2 = 0; }
  reset() { this.z1 = this.z2 = 0; }
  process(x) { const y = this.b0 * x + this.z1; this.z1 = this.b1 * x - this.a1 * y + this.z2; this.z2 = this.b2 * x - this.a2 * y; return y; }
  setN(B0, B1, B2, A0, A1, A2) { this.b0 = B0 / A0; this.b1 = B1 / A0; this.b2 = B2 / A0; this.a1 = A1 / A0; this.a2 = A2 / A0; }
  static w0(fs, f) { return 2 * PI * Math.min(f, fs * 0.49) / fs; }
  lowpass(fs, f, q) { const w = Biquad.w0(fs, f), c = Math.cos(w), al = Math.sin(w) / (2 * q); this.setN((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al); }
  highpass(fs, f, q) { const w = Biquad.w0(fs, f), c = Math.cos(w), al = Math.sin(w) / (2 * q); this.setN((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al); }
  peak(fs, f, q, db) { const A = Math.pow(10, db / 40), w = Biquad.w0(fs, f), c = Math.cos(w), al = Math.sin(w) / (2 * q); this.setN(1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A); }
  lowShelf(fs, f, db) { const A = Math.pow(10, db / 40), w = Biquad.w0(fs, f), c = Math.cos(w), s = Math.sin(w), al = s / 2 * Math.SQRT2, sa = 2 * Math.sqrt(A) * al;
    this.setN(A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa), (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa); }
  highShelf(fs, f, db) { const A = Math.pow(10, db / 40), w = Biquad.w0(fs, f), c = Math.cos(w), s = Math.sin(w), al = s / 2 * Math.SQRT2, sa = 2 * Math.sqrt(A) * al;
    this.setN(A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa), (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa); }
  copyFrom(o) { this.b0 = o.b0; this.b1 = o.b1; this.b2 = o.b2; this.a1 = o.a1; this.a2 = o.a2; }
}
class OnePole {
  constructor() { this.a = 0; this.z = 0; }
  setLP(fs, f) { this.a = Math.exp(-2 * PI * Math.min(f, fs * 0.49) / fs); }
  lp(x) { this.z = x + this.a * (this.z - x); return this.z; }
  hp(x) { return x - this.lp(x); }
  reset() { this.z = 0; }
}
class DelayLine {
  constructor(n) { this.buf = new Float32Array(Math.max(4, n | 0)); this.w = 0; }
  push(x) { this.buf[this.w] = x; if (++this.w >= this.buf.length) this.w = 0; }
  read(d) { const n = this.buf.length; d = Math.min(Math.max(d, 1), n - 2); let rp = this.w - 1 - d; while (rp < 0) rp += n;
    let i0 = rp | 0; if (i0 >= n) i0 -= n; const i1 = (i0 + 1) % n, fr = rp - Math.floor(rp); return this.buf[i0] + fr * (this.buf[i1] - this.buf[i0]); }
}
class Reverb {
  prepare(fs) {
    this.fs = fs; const s = fs / 44100; const c = [1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617], a = [556, 441, 341, 225];
    const comb = n => ({ b: new Float32Array(Math.floor(n)), i: 0, store: 0 }), ap = n => ({ b: new Float32Array(Math.floor(n)), i: 0 });
    this.cl = c.map(v => comb(v * s)); this.cr = c.map(v => comb((v + 23) * s));
    this.al = a.map(v => ap(v * s)); this.ar = a.map(v => ap((v + 23) * s));
    this.pre = new DelayLine(fs * 0.05);
  }
  process(inp, size01, tone01, out) {
    this.pre.push(inp);
    const x = this.pre.read(this.fs * 0.018) * 0.015, fb = 0.70 + 0.28 * size01, damp = 0.05 + 0.6 * (1 - tone01);
    let l = 0, r = 0;
    for (let k = 0; k < 8; ++k) {
      let C = this.cl[k], o = C.b[C.i]; C.store = o * (1 - damp) + C.store * damp; C.b[C.i] = x + C.store * fb; if (++C.i >= C.b.length) C.i = 0; l += o;
      C = this.cr[k]; o = C.b[C.i]; C.store = o * (1 - damp) + C.store * damp; C.b[C.i] = x + C.store * fb; if (++C.i >= C.b.length) C.i = 0; r += o;
    }
    for (let k = 0; k < 4; ++k) {
      let A = this.al[k], bo = A.b[A.i], o = -l + bo; A.b[A.i] = l + bo * 0.5; if (++A.i >= A.b.length) A.i = 0; l = o;
      A = this.ar[k]; bo = A.b[A.i]; o = -r + bo; A.b[A.i] = r + bo * 0.5; if (++A.i >= A.b.length) A.i = 0; r = o;
    }
    out[0] = l * 3; out[1] = r * 3;
  }
}

const DRIVE_RANGE = [[6, 36], [0, 30], [0, 18], [18, 44]];
const CAB_OFF = ANJI_CABS.length - 1;

class AnjiEngine {
  constructor(fs) {
    this.fs = fs; this.fsO = fs * 2;
    this.p = {}; for (const i of ANJI_PARAMS) this.p[i.id] = i.def;
    const q = [0.5176380902, 0.7071067812, 1.9318516526];
    this.up = q.map(v => { const b = new Biquad(); b.lowpass(fs * 2, fs * 0.45, v); return b; });
    this.dn = q.map(v => { const b = new Biquad(); b.lowpass(fs * 2, fs * 0.45, v); return b; });
    this.pre = [new Biquad(), new Biquad()];
    this.stLpf = [0, 1, 2, 3].map(() => new Biquad()); this.stHpf = [0, 1, 2, 3].map(() => new Biquad());
    this.stack = [0, 1, 2].map(() => new Biquad()); this.pwr = [new Biquad(), new Biquad()];
    this.cabF = [0, 1, 2, 3, 4, 5].map(() => new Biquad());
    this.odHp = new OnePole(); this.odHp2 = new OnePole(); this.odTone = new OnePole(); this.odTone2 = new OnePole();
    this.dlyLp = new OnePole(); this.dlyHp = new OnePole();
    this.delay = new DelayLine(fs * 2.1); this.chorus = new DelayLine(fs * 0.05);
    this.reverb = new Reverb(); this.reverb.prepare(fs); this.rv = [0, 0];
    this.gateEnv = 0; this.gateGain = 1; this.gateHold = 0; this.compEnv = 0; this.sagEnv = 0;
    this.tremPh = 0; this.chorPh = 0; this.curDelay = -1;
    this.dirty = true;
  }
  set(id, v) { if (this.p[id] !== v) { this.p[id] = v; this.dirty = true; } }
  setAll(values) { for (const i of ANJI_PARAMS) this.p[i.id] = (values && values[i.id] !== undefined) ? values[i.id] : i.def; this.dirty = true; }

  update() {
    this.dirty = false; const p = this.p, fs = this.fs, fsO = this.fsO;
    const ampI = Math.min(Math.max(Math.round(p.amp), 0), ANJI_AMPS.length - 1); this.ampI = ampI;
    const a = ANJI_AMPS[ampI]; this.a = a;
    const cabI = Math.min(Math.max(Math.round(p.cab), 0), ANJI_CABS.length - 1);
    this.odMode = Math.min(Math.max(Math.round(p.od_mode), 0), 3);
    const g01 = clamp01(p.gain / 10);
    this.inG = dbToGain(p.in_gain); this.outG = dbToGain(p.out_gain);
    this.gateOpenT = dbToGain(-80 + 5 * p.gate_thresh); this.gateCloseT = this.gateOpenT * 0.6; this.gateRel = Math.exp(-1 / (fs * 0.03));
    this.compTh = -8 - 3.2 * p.comp_amount; this.compRatio = 3 + 0.5 * p.comp_amount;
    this.compMakeup = dbToGain(-this.compTh * (1 - 1 / this.compRatio) * 0.55 + (p.comp_level - 5) * 1.2);
    this.compAtt = 1 - Math.exp(-1 / (fs * 0.004)); this.compRelC = 1 - Math.exp(-1 / (fs * 0.15));
    const dr = DRIVE_RANGE[this.odMode];
    this.odGain = dbToGain(dr[0] + (dr[1] - dr[0]) * p.od_drive / 10); this.odOut = dbToGain((p.od_level - 5) * 2.4); this.odToneAmt = clamp01(p.od_tone / 10);
    switch (this.odMode) {
      case 0: this.odHp.setLP(fsO, 720); this.odTone.setLP(fsO, 700 * Math.pow(7, this.odToneAmt)); break;
      case 1: this.odHp.setLP(fsO, 300); this.odTone.setLP(fsO, 1500); break;
      case 2: this.odHp.setLP(fsO, 1200); this.odTone.setLP(fsO, 3000 + 9000 * this.odToneAmt); break;
      default: this.odHp.setLP(fsO, 120); this.odHp2.setLP(fsO, 150); this.odTone.setLP(fsO, 900); this.odTone2.setLP(fsO, 1100);
    }
    this.preG = dbToGain(a.gainMinDb + (a.gainMaxDb - a.gainMinDb) * g01);
    this.pre[0].highpass(fsO, a.inHpf, 0.7);
    this.pre[1].highShelf(fsO, 2500, p.bright > 0.5 ? a.brightDb * (1 - 0.7 * g01) : 0);
    for (let s = 0; s < a.stages; ++s) { this.stLpf[s].lowpass(fsO, a.stLpf[s], 0.6); this.stHpf[s].highpass(fsO, a.stHpf[s], 0.6); }
    this.stack[0].lowShelf(fsO, a.bassF, (p.bass - 5) / 5 * a.bassRange);
    this.stack[1].peak(fsO, a.midF, a.midQ, a.midBase + (p.mid - 5) / 5 * a.midRange);
    this.stack[2].highShelf(fsO, a.trebF, (p.treble - 5) / 5 * a.trebRange);
    this.pwrDrive = dbToGain(-20 + 3.2 * p.master) * a.powerDrive;
    this.pwr[0].highShelf(fsO, a.presF, (p.presence - 5) * 1.6);
    this.pwr[1].peak(fsO, a.resF, 0.8, (p.resonance - 5) * 1.4);
    this.sagAtt = 1 - Math.exp(-1 / (fsO * 0.004)); this.sagRel = 1 - Math.exp(-1 / (fsO * 0.12));
    this.ampTrim = dbToGain(a.trimDb);
    this.useCab = cabI !== CAB_OFF;
    if (this.useCab) {
      const c = ANJI_CABS[cabI], m = clamp01(p.mic / 10);
      this.cabF[0].highpass(fs, c.hpf, 0.8); this.cabF[1].peak(fs, c.resF, 1.4, c.resDb); this.cabF[2].peak(fs, c.p1F, c.p1Q, c.p1Db);
      this.cabF[3].peak(fs, c.p2F, c.p2Q, c.p2Db + 3 * (1 - m) - 2 * m);
      this.cabF[4].lowpass(fs, c.lpf * (1.35 - 0.6 * m), 0.75); this.cabF[5].lowpass(fs, c.lpf * (1.6 - 0.6 * m), 1.0);
    }
    this.tremInc = Math.max(0.1, p.trem_rate) / fs; this.tremDepth = clamp01(p.trem_depth / 10) * 0.9;
    this.chorInc = Math.max(0.05, p.chorus_rate) / fs; this.chorDepth = clamp01(p.chorus_depth / 10); this.chorMix = clamp01(p.chorus_mix / 10);
    this.dlyTarget = Math.min(Math.max(p.delay_time, 20), 2000) * 0.001 * fs;
    this.dlyFb = clamp01(p.delay_fb / 10) * 0.92; this.dlyMix = clamp01(p.delay_mix / 10) * 0.9;
    this.dlyLp.setLP(fs, 1500 + 8000 * clamp01(p.delay_tone / 10)); this.dlyHp.setLP(fs, 90);
    this.revSize = clamp01(p.reverb_size / 10); this.revTone = clamp01(p.reverb_tone / 10); this.revMix = clamp01(p.reverb_mix / 10);
    this.on = { gate: p.gate_on > 0.5, comp: p.comp_on > 0.5, od: p.od_on > 0.5, trem: p.trem_on > 0.5, chorus: p.chorus_on > 0.5, delay: p.delay_on > 0.5, reverb: p.reverb_on > 0.5 };
  }

  drive(x) {
    switch (this.odMode) {
      case 0: { const c = this.odHp.hp(x); let y = x + ftanh(c * this.odGain) * 0.6; y = this.odTone.lp(y); return y * this.odOut; }
      case 1: { const c = this.odHp.hp(x); let v = c * this.odGain; v = v / (1 + Math.abs(v)); let y = x * 0.8 + v * 0.5;
        y = y + (y - this.odTone.lp(y)) * (this.odToneAmt - 0.5) * 1.5; return y * this.odOut; }
      case 2: { const c = x * 0.4 + this.odHp.hp(x) * this.odGain; let y = tube(c * 1.5, 0.25); y = this.odTone.lp(y); return y * this.odOut; }
      default: { let v = this.odHp.hp(x) * this.odGain; v = ftanh(v); v = this.odHp2.hp(v); v = ftanh(v * 6 + 0.1) - 0.0997;
        const lo = this.odTone.lp(v), hi = v - this.odTone2.lp(v); return (lo * (1 - this.odToneAmt) + hi * this.odToneAmt * 1.6) * this.odOut; }
    }
  }
  nonlinear(x) {
    const a = this.a;
    if (this.on.od) x = this.drive(x);
    x = this.pre[0].process(x) * this.preG; x = this.pre[1].process(x);
    for (let s = 0; s < a.stages; ++s) {
      x *= a.stGain[s]; x = a.solidState ? solidClip(x) : tube(x, a.stBias[s]);
      x = this.stHpf[s].process(x); x = this.stLpf[s].process(x);
    }
    x = this.stack[0].process(x); x = this.stack[1].process(x); x = this.stack[2].process(x);
    const ax = Math.abs(x);
    this.sagEnv = ax > this.sagEnv ? this.sagEnv + (ax - this.sagEnv) * this.sagAtt : this.sagEnv + (ax - this.sagEnv) * this.sagRel;
    let y = x * this.pwrDrive / (1 + a.sag * this.sagEnv * 2);
    y = this.pwr[0].process(y); y = this.pwr[1].process(y);
    return a.solidState ? solidClip(y) : tube(y, 0.06);
  }
  // inp: Float32Array mono; outL/outR: Float32Array
  process(inp, outL, outR) {
    if (this.dirty) this.update();
    const n = inp.length, fs = this.fs, on = this.on, up = this.up, dn = this.dn, rv = this.rv;
    for (let i = 0; i < n; ++i) {
      let x = inp[i] * this.inG;
      if (on.gate) {
        const ax = Math.abs(x);
        this.gateEnv = ax > this.gateEnv ? ax : this.gateEnv * this.gateRel;
        if (this.gateEnv > this.gateOpenT) this.gateHold = (fs * 0.05) | 0; else if (this.gateHold > 0) --this.gateHold;
        const t = (this.gateEnv > this.gateOpenT || this.gateHold > 0) ? 1 : (this.gateEnv < this.gateCloseT ? 0 : this.gateGain);
        this.gateGain += (t - this.gateGain) * (t > this.gateGain ? 0.05 : 0.0015); x *= this.gateGain;
      }
      if (on.comp) {
        const ax = Math.abs(x);
        this.compEnv = ax > this.compEnv ? this.compEnv + (ax - this.compEnv) * this.compAtt : this.compEnv + (ax - this.compEnv) * this.compRelC;
        const envDb = gainToDb(this.compEnv);
        const gr = envDb > this.compTh ? (this.compTh + (envDb - this.compTh) / this.compRatio) - envDb : 0;
        x *= dbToGain(gr) * this.compMakeup;
      }
      let u0 = x * 2, u1 = 0;
      for (let k = 0; k < 3; ++k) u0 = up[k].process(u0);
      for (let k = 0; k < 3; ++k) u1 = up[k].process(u1);
      let y0 = this.nonlinear(u0), y1 = this.nonlinear(u1);
      for (let k = 0; k < 3; ++k) y0 = dn[k].process(y0);
      for (let k = 0; k < 3; ++k) y1 = dn[k].process(y1);
      let y = y1;
      if (this.useCab) for (let k = 0; k < 6; ++k) y = this.cabF[k].process(y);
      x = y * this.ampTrim;

      // ---- post
      if (on.trem) {
        this.tremPh += this.tremInc; if (this.tremPh > 1) this.tremPh -= 1;
        const s = 0.5 + 0.5 * Math.sin(2 * PI * this.tremPh); x *= 1 - this.tremDepth * s * s * (3 - 2 * s);
      }
      let l = x, r = x;
      if (on.chorus) {
        this.chorus.push(x); this.chorPh += this.chorInc; if (this.chorPh > 1) this.chorPh -= 1;
        const base = fs * 0.007, dep = fs * 0.0035 * this.chorDepth;
        const wl = this.chorus.read(base + dep * Math.sin(2 * PI * this.chorPh)), wr = this.chorus.read(base + dep * Math.sin(2 * PI * this.chorPh + PI * 0.5));
        l = x * (1 - this.chorMix * 0.5) + wl * this.chorMix * 0.7; r = x * (1 - this.chorMix * 0.5) + wr * this.chorMix * 0.7;
      }
      if (on.delay) {
        if (this.curDelay < 0) this.curDelay = this.dlyTarget;
        this.curDelay += (this.dlyTarget - this.curDelay) * 0.0002;
        const d1 = this.delay.read(this.curDelay), d2 = this.delay.read(this.curDelay + fs * 0.011);
        let fb = this.dlyHp.hp(this.dlyLp.lp(d1)) * this.dlyFb; fb = ftanh(fb * 1.2) / 1.2;
        this.delay.push((l + r) * 0.5 + fb); l += d1 * this.dlyMix; r += d2 * this.dlyMix;
      }
      if (on.reverb) {
        this.reverb.process((l + r) * 0.5, this.revSize, this.revTone, rv);
        l = l * (1 - this.revMix * 0.3) + rv[0] * this.revMix; r = r * (1 - this.revMix * 0.3) + rv[1] * this.revMix;
      }
      outL[i] = l * this.outG; outR[i] = r * this.outG;
    }
  }
}
global.AnjiEngine = AnjiEngine;
})(typeof globalThis !== 'undefined' ? globalThis : this);
