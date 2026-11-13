const fs = require('fs'), vm = require('vm'), dir = process.argv[2];
for (const f of ['models.js', 'presets.js', 'engine.js']) vm.runInThisContext(fs.readFileSync(dir + '/' + f, 'utf8'));
const di = new Float32Array(fs.readFileSync('/tmp/di.f32').buffer.slice(0)); const N = di.length;
const cpp = new Float32Array(fs.readFileSync('/tmp/cpp_out.f32').buffer.slice(0));
let worst = 0;
ANJI_DATA.presets.forEach((p, idx) => {
  const e = new AnjiEngine(48000); e.setAll(p.values);
  const L = new Float32Array(N), R = new Float32Array(N);
  for (let pos = 0; pos < N; pos += 128) e.process(di.subarray(pos, pos + 128), L.subarray(pos, pos + 128), R.subarray(pos, pos + 128));
  let se = 0, ss = 0; const off = idx * 2 * N;
  for (let i = 0; i < N; i++) { const a = cpp[off + i], b = cpp[off + N + i]; se += (L[i] - a) ** 2 + (R[i] - b) ** 2; ss += a * a + b * b; }
  const err = 10 * Math.log10(se / ss + 1e-30); worst = Math.max(worst, err);
  console.log(p.name.padEnd(24), 'error vs C++:', err.toFixed(1), 'dB');
});
console.log('worst', worst.toFixed(1), 'dB');
