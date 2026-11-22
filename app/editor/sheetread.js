// SPDX-License-Identifier: MIT
// MB-1's patch editor: a patch read off a picture of a patch sheet. Everything happens in the browser.
//  - MB-1's own sheets (the editor's SVG and PNG): the patch rides inside them (SVG <metadata id="mb1-patch">, a PNG
//    tEXt chunk "mb1-patch"), as the hex of SysEx frame 01: read back exactly.
//  - electric-himalaya.com's MiniBrute sheets (982 x 631, yellow dots on dark knobs): the dots, the faders' yellow
//    caps, the switches' dots, the filter mode's letters and the LFO's wave icon.
//  - Ultimate Patches' sheets (the Arturia panel drawn, an olive frame; PDF pages or pictures of them): the knobs'
//    white pointers, the faders' grey caps, the toggles' levers; a PDF's page title as the patch's name.
// A MiniBrute control MB-1 has not (PHONES, MASTER, FINE TUNE, AUDIO IN, ENV SPEED, the LFO's CLOCK, TEMPO, OCTAVE)
// is left out; one not marked on the sheet keeps INIT's value. Each value says how it was read: "ok", "guess" (a
// choice told apart by its shape, or a switch whose order is not certain), "unset" (not on the sheet: INIT).

export const NP = 76;
// parameter numbers (app/mbpatch.h's order)
const P = Object.fromEntries(["CUT","RES","BRUTE","FENV","MODE","KBD","FVEL","AVEL","SAW","SQR","TRI","SUB","NOISE","ULTRA","PW","METAL",
  "FA","FD","FS","FR","AA","AD","AS","AR","LRATE","LWAVE","LPITCH","LFILT","LPWM","LAMP","VRATE","VTYPE",
  "GLIDE","VINT","WDEST","WAMT","SUBOCT","SUBSHP","USRATE","LEVEL","ATVIB","ATCUT","PWENV","MTENV","PLAY","BEND","LRTRG",
  "ARP","AMODE","AOCT","ASTEP","ASWING","DTIME","DFB","DTONE","DMIX","DSYNC","DMODE","RDECAY","RSIZE","RDAMP","RMIX","RPRE","FXORD",
  "XTYPE","XDRV","XTONE","XMIX","CRATE","CDEPTH","CPHASE","CMIX","KTHR","KRAT","KATT","KREL"].map((n, i) => [n, i]));
export { P };

const HEX = (a) => Array.from(a, (b) => b.toString(16).padStart(2, "0")).join("");
const UNHEX = (s) => Uint8Array.from(s.match(/../g) || [], (h) => parseInt(h, 16));

/* ------------------------------------------------------------ MB-1's own sheets: the patch inside the file */
export function svgWithPatch(svg, frame) {
  return svg.replace(/^(<svg[^>]*>)/, (m) => `${m}<metadata id="mb1-patch">${HEX(frame)}</metadata>`);
}
const CRC = (() => { const t = new Uint32Array(256); for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; t[n] = c >>> 0; } return t; })();
const crc32 = (b) => { let c = 0xFFFFFFFF; for (const x of b) c = CRC[(c ^ x) & 255] ^ (c >>> 8); return (c ^ 0xFFFFFFFF) >>> 0; };
export async function pngWithPatch(blob, frame) {   /* a tEXt chunk after IHDR */
  const b = new Uint8Array(await blob.arrayBuffer());
  const text = new TextEncoder().encode("mb1-patch\0" + HEX(frame));
  const type = new TextEncoder().encode("tEXt");
  const chunk = new Uint8Array(12 + text.length), dv = new DataView(chunk.buffer);
  dv.setUint32(0, text.length); chunk.set(type, 4); chunk.set(text, 8);
  dv.setUint32(8 + text.length, crc32(chunk.subarray(4, 8 + text.length)));
  const at = 8 + 25;   /* the signature, then IHDR (13 bytes of data) */
  const out = new Uint8Array(b.length + chunk.length);
  out.set(b.subarray(0, at)); out.set(chunk, at); out.set(b.subarray(at), at + chunk.length);
  return new Blob([out], { type: "image/png" });
}
function pngPatch(b) {
  if (b[0] !== 0x89 || b[1] !== 0x50) return null;
  let i = 8;
  const dv = new DataView(b.buffer, b.byteOffset, b.byteLength);
  while (i + 8 <= b.length) {
    const len = dv.getUint32(i), type = String.fromCharCode(...b.subarray(i + 4, i + 8));
    if (type === "tEXt") {
      const s = new TextDecoder("latin1").decode(b.subarray(i + 8, i + 8 + len));
      if (s.startsWith("mb1-patch\0")) return UNHEX(s.slice(10));
    }
    if (type === "IEND") break;
    i += 12 + len;
  }
  return null;
}
function framePatch(f) {   /* SysEx 01 or 07 -> { name, v } */
  if (!f || f[0] !== 0xF0 || f[1] !== 0x7D || f[2] !== 0x4D || f[3] !== 0x42) return null;
  const o = f[4] === 7 ? 5 : 6;
  return { name: String.fromCharCode(...f.slice(o, o + 8)), v: Array.from(f.slice(o + 8, o + 8 + NP)) };
}

/* ------------------------------------------------------------ pictures */
function canvasOf(src, w, h) {
  const c = document.createElement("canvas"); c.width = w; c.height = h;
  const x = c.getContext("2d", { willReadFrequently: true }); x.imageSmoothingQuality = "high";
  return [c, x];
}
async function imageOf(blob) {
  const url = URL.createObjectURL(blob);
  try { const im = new Image(); im.src = url; await im.decode(); return im; } finally { setTimeout(() => URL.revokeObjectURL(url), 1000); }
}
/* the pixels of the region (sx, sy, sw, sh) of src scaled to w x h */
function pixels(src, sx, sy, sw, sh, w, h) {
  const [c, x] = canvasOf(null, w, h);
  x.fillStyle = "#fff"; x.fillRect(0, 0, w, h);
  x.drawImage(src, sx, sy, sw, sh, 0, 0, w, h);
  return { canvas: c, w, h, d: x.getImageData(0, 0, w, h).data };
}
const at = (im, x, y) => { x |= 0; y |= 0; const i = (y * im.w + x) * 4; return [im.d[i], im.d[i + 1], im.d[i + 2]]; };
const deg = (dx, dy) => Math.atan2(dx, -dy) * 180 / Math.PI;   /* 0 = up, clockwise */
const clamp01 = (x) => Math.max(0, Math.min(1, x));
function blob(im, x0, y0, x1, y1, test) {   /* the pixels in the box that pass: count, centroid, the pixels */
  let n = 0, sx = 0, sy = 0; const pts = [];
  for (let y = Math.max(0, y0 | 0); y < Math.min(im.h, y1); y++)
    for (let x = Math.max(0, x0 | 0); x < Math.min(im.w, x1); x++) {
      const [r, g, b] = at(im, x, y);
      if (test(r, g, b)) { n++; sx += x; sy += y; pts.push([x, y]); }
    }
  return { n, x: n ? sx / n : 0, y: n ? sy / n : 0, pts };
}

/* the knob's angle -> its value: a continuous knob over -135 .. +135; a choice: the nearest of its positions */
const knobV = (a) => Math.round(clamp01((a + 135) / 270) * 127);
const nearest = (a, angles) => angles.reduce((bi, x, i) => (Math.abs(a - x) < Math.abs(a - angles[bi]) ? i : bi), 0);
/* the MiniBrute's own rotary switches: where each position is printed (degrees, 0 up) */
const POS = {
  MODE: [-10, 30, 62, 95],                       /* LP BP HP NOTCH */
  LWAVE: [0, 40, 70, 100, 125, 150],              /* sine tri saw square S&H random(smooth) */
  ASTEP: [-10, 22, 52, 82, 112, 142],             /* 1/4 1/8 1/16 1/4T 1/8T 1/16T */
  ASWING: [0, 30, 60, 90, 120, 150],
  AOCT: [-10, 30, 60, 90],
  AMODE: [0, 30, 60, 90],                         /* up down up/dn random */
};

/* the MiniBrute's switches, top to bottom, as MB-1's values (guess: the order of a three-way icon switch) */
const SWITCH = {
  SUBSHP: { n: 2, to: (i) => ({ [P.SUBSHP]: i }) },                          /* square, sine */
  SUBOCT: { n: 2, to: (i) => ({ [P.SUBOCT]: i }) },                          /* -1, -2 */
  WDEST: { n: 3, to: (i) => ({ [P.WDEST]: 2 - i }) },                        /* cutoff, vibrato, LFO amt */
  AT: { n: 3, to: (i) => ({ [P.ATCUT]: i === 0 ? 127 : 0, [P.ATVIB]: i === 1 ? 127 : 0 }) },   /* cutoff, vibrato, off */
  VTYPE: { n: 3, to: (i) => ({ [P.VTYPE]: 2 - i }), guess: true },           /* +sqr, sine, -sqr */
  ARP: { n: 3, to: (i) => ({ [P.ARP]: 2 - i }) },                            /* hold, on, off */
};

/* ------------------------------------------------------------ electric-himalaya.com (982 x 631) */
const EH = {
  w: 982, h: 631,
  knobs: [["ULTRA", 149.5, 66.4], ["PW", 246.3, 66.4], ["METAL", 342.4, 66.3], ["CUT", 438.6, 63.2], ["RES", 535.2, 63.6],
    ["BRUTE", 734, 65.5], ["USRATE", 149.6, 154.7], ["PWENV", 246.3, 155.4], ["MTENV", 342.4, 155.2], ["FENV", 438.7, 155.2],
    ["KBD", 534.7, 155.2], ["LPWM", 342.2, 483.6], ["LPITCH", 438.5, 483.5], ["LFILT", 534.5, 483.5], ["LAMP", 631.2, 483.8],
    ["ASTEP", 825.5, 483], ["BEND", 53.8, 572.7], ["GLIDE", 149.2, 572.7], ["VRATE", 245.9, 572.6], ["LRATE", 440.5, 572.5],
    ["AOCT", 631.1, 572.8], ["AMODE", 728, 573], ["ASWING", 823.8, 572.7]],
  faders: [["SUB", 53.5], ["SAW", 111.5], ["SQR", 168.5], ["TRI", 226.5], ["NOISE", 284.5],
    ["FA", 457.5], ["FD", 515.5], ["FS", 573.5], ["FR", 631.5], ["AA", 746.5], ["AD", 804.5], ["AS", 862.5], ["AR", 920.5]],
  fTop: 245, fBot: 392,
  switches: [["SUBSHP", 53, 49, 85], ["SUBOCT", 53, 138, 174], ["WDEST", 53, 466, 503], ["AT", 149, 465, 503],
    ["VTYPE", 245, 466, 503], ["ARP", 728, 466, 503]],
  mode: [612, 52, 652, 76], wave: [316, 548, 368, 590],
};
const yellow = (r, g, b) => r > 190 && g > 160 && b < 110 && r - b > 100;
function isEH(src) {
  const ratio = src.width / src.height;
  if (Math.abs(ratio - EH.w / EH.h) > 0.06) return false;
  const im = pixels(src, 0, 0, src.width, src.height, EH.w, EH.h);
  let grey = 0, dark = 0, n = 0;
  for (let y = 0; y < EH.h; y += 4) for (let x = 0; x < EH.w; x += 4) {
    const [r, g, b] = at(im, x, y); n++;
    if (Math.abs(r - g) < 12 && Math.abs(g - b) < 12) { if (r > 110 && r < 175) grey++; else if (r < 70) dark++; }
  }
  return grey / n > 0.3 && dark / n > 0.12;
}
function letterOf(im, box) {   /* the filter mode printed in yellow: its first letter, L B H or N */
  const b = blob(im, box[0], box[1], box[2], box[3], yellow);
  if (b.n < 20) return null;
  const xs = b.pts.map((p) => p[0]), x0 = Math.min(...xs);
  /* the first glyph: the pixels left of the first empty column after x0 */
  let x1 = x0 + 1; const has = new Set(xs);
  while (has.has(x1)) x1++;
  const g = b.pts.filter((p) => p[0] < x1), gx0 = x0, gx1 = x1 - 1;
  const gy = g.map((p) => p[1]), gy0 = Math.min(...gy), gy1 = Math.max(...gy), W = gx1 - gx0 + 1, H = gy1 - gy0 + 1;
  const row = (fy) => { const yy = Math.round(gy0 + fy * (H - 1)); return new Set(g.filter((p) => Math.abs(p[1] - yy) <= 0).map((p) => p[0])).size / W; };
  const top = row(0), mid = row(0.5), bot = row(1);
  if (top < 0.5 && mid < 0.5 && bot > 0.6) return [0, "ok"];               /* L */
  if (top > 0.55 && bot > 0.55) return [1, "ok"];                           /* B */
  if (mid > 0.6) return [2, "ok"];                                          /* H */
  return [3, "guess"];                                                      /* N */
}
function waveOf(im, box) {   /* the LFO's wave icon, by its shape: sine tri saw square S&H random */
  const b = blob(im, box[0], box[1], box[2], box[3], yellow);
  if (b.n < 30) return null;
  const xs = b.pts.map((p) => p[0]), x0 = Math.min(...xs), x1 = Math.max(...xs);
  const ys = b.pts.map((p) => p[1]), y0 = Math.min(...ys), y1 = Math.max(...ys);
  const N = x1 - x0 + 1, prof = [], spread = [];
  for (let x = x0; x <= x1; x++) {
    const c = b.pts.filter((p) => p[0] === x).map((p) => p[1]);
    prof.push(c.length ? 1 - (c.reduce((a, v) => a + v, 0) / c.length - y0) / Math.max(1, y1 - y0) : null);
    spread.push(c.length ? (Math.max(...c) - Math.min(...c)) / Math.max(1, y1 - y0) : 0);
  }
  const t = (f) => prof.map((_, i) => f(i / Math.max(1, N - 1)));
  const tmpl = [t((u) => 0.5 + 0.5 * Math.sin(2 * Math.PI * u)), t((u) => 1 - Math.abs(2 * u - 1)), t((u) => u),
    t((u) => (u < 0.5 ? 1 : 0)), t((u) => [0.2, 0.8, 0.4, 1, 0.1, 0.6][Math.min(5, Math.floor(u * 6))]), t((u) => 0.5 + 0.3 * Math.sin(6 * u) * Math.cos(11 * u))];
  const corr = (a) => {
    const pts = prof.map((v, i) => [v, a[i]]).filter((p) => p[0] != null);
    const ma = pts.reduce((s, p) => s + p[0], 0) / pts.length, mb = pts.reduce((s, p) => s + p[1], 0) / pts.length;
    let sab = 0, saa = 0, sbb = 0; for (const [x, y] of pts) { sab += (x - ma) * (y - mb); saa += (x - ma) ** 2; sbb += (y - mb) ** 2; }
    return sab / Math.sqrt(saa * sbb + 1e-9);
  };
  const tall = spread.filter((s) => s > 0.6).length;   /* vertical edges: a square, a saw's drop, steps */
  const sc = tmpl.map(corr);
  if (tall >= 2 && sc[3] > 0.5) sc[3] += 0.3;
  let k = sc.indexOf(Math.max(...sc));
  return [k, sc[k] > 0.8 ? "ok" : "guess"];
}
function readEH(src) {
  const im = pixels(src, 0, 0, src.width, src.height, EH.w, EH.h);
  const v = {}, how = {}, marks = [];
  for (const [n, cx, cy] of EH.knobs) {
    const b = blob(im, cx - 46, cy - 46, cx + 47, cy + 47, (r, g, bb) => yellow(r, g, bb));
    const pts = b.pts.filter(([x, y]) => { const d = Math.hypot(x - cx, y - cy); return d > 18 && d < 47; });
    if (pts.length < 12) { marks.push({ x: cx, y: cy, n, how: "unset" }); continue; }
    const mx = pts.reduce((s, p) => s + p[0], 0) / pts.length, my = pts.reduce((s, p) => s + p[1], 0) / pts.length;
    const a = deg(mx - cx, my - cy);
    if (POS[n]) { v[P[n]] = nearest(a, POS[n]); how[P[n]] = "ok"; }
    else if (n === "BEND") { v[P.BEND] = Math.round(clamp01((a + 135) / 270) * 11); how[P.BEND] = "ok"; }
    else { v[P[n]] = knobV(a); how[P[n]] = "ok"; }
    marks.push({ x: cx, y: cy, n, how: how[P[n]] });
  }
  for (const [n, cx] of EH.faders) {
    const b = blob(im, cx - 16, EH.fTop - 14, cx + 17, EH.fBot + 14, yellow);
    if (b.n < 40) { marks.push({ x: cx, y: (EH.fTop + EH.fBot) / 2, n, how: "unset" }); continue; }
    v[P[n]] = Math.round(clamp01((EH.fBot - b.y) / (EH.fBot - EH.fTop)) * 127); how[P[n]] = "ok";
    marks.push({ x: cx, y: b.y, n, how: "ok" });
  }
  for (const [n, cx, y0, y1] of EH.switches) {
    const s = SWITCH[n], b = blob(im, cx - 10, y0, cx + 11, y1 + 1, yellow);
    if (b.n < 25) { marks.push({ x: cx, y: (y0 + y1) / 2, n, how: "unset" }); continue; }
    const i = Math.round(clamp01((b.y - (y0 + 8)) / (y1 - y0 - 16)) * (s.n - 1));
    for (const [p, x] of Object.entries(s.to(i))) { v[p] = x; how[p] = s.guess ? "guess" : "ok"; }
    marks.push({ x: cx, y: b.y, n, how: s.guess ? "guess" : "ok" });
  }
  const m = letterOf(im, EH.mode);
  if (m) { v[P.MODE] = m[0]; how[P.MODE] = m[1]; }
  marks.push({ x: (EH.mode[0] + EH.mode[2]) / 2, y: (EH.mode[1] + EH.mode[3]) / 2, n: "MODE", how: m ? m[1] : "unset" });
  const w = waveOf(im, EH.wave);
  if (w) { v[P.LWAVE] = w[0]; how[P.LWAVE] = w[1]; }
  marks.push({ x: (EH.wave[0] + EH.wave[2]) / 2, y: (EH.wave[1] + EH.wave[3]) / 2, n: "LWAVE", how: w ? w[1] : "unset" });
  return { layout: "electric-himalaya.com sheet", canvas: im.canvas, v, how, marks };
}

/* ------------------------------------------------------------ Ultimate Patches (the Arturia panel in an olive frame) */
/* coordinates in a page rendered 1650 px wide (150 dpi), the frame from (52, 246) to (1592, 1010) */
const UP = {
  f: [52, 246, 1592, 1010],
  knobs: [["ULTRA", 580, 329], ["PW", 697, 330], ["METAL", 814, 330], ["CUT", 930, 331], ["RES", 1046, 329], ["MODE", 1156, 330],
    ["BRUTE", 1279, 329], ["USRATE", 580, 437], ["PWENV", 696, 438], ["MTENV", 812, 437], ["FENV", 931, 437], ["KBD", 1047, 438],
    ["LPWM", 814, 834], ["LPITCH", 930, 836], ["LFILT", 1047, 835], ["LAMP", 1163, 835], ["ASTEP", 1396, 835],
    ["BEND", 465, 942], ["GLIDE", 581, 942], ["VRATE", 698, 942], ["LWAVE", 813, 942], ["LRATE", 932, 942],
    ["AOCT", 1163, 942], ["AMODE", 1280, 942], ["ASWING", 1397, 943]],
  faders: [["SUB", 464], ["SAW", 534], ["SQR", 604], ["TRI", 674], ["NOISE", 744],
    ["FA", 953], ["FD", 1024], ["FS", 1094], ["FR", 1164], ["AA", 1303], ["AD", 1374], ["AS", 1444], ["AR", 1514]],
  fTop: 567, fBot: 705,
  switches: [["SUBSHP", 462, 296, 360], ["SUBOCT", 462, 408, 464], ["WDEST", 463, 803, 864], ["AT", 578, 803, 864],
    ["VTYPE", 695, 803, 864], ["ARP", 1280, 803, 864]],
};
const olive = (r, g, b) => r > 140 && g > 135 && b < 110 && r - b > 70 && Math.abs(r - g) < 45;
function frameUP(src) {   /* the olive frame's outer edges, or null */
  const W = 825, H = Math.round(src.height * W / src.width);
  const im = pixels(src, 0, 0, src.width, src.height, W, H);
  const rows = new Array(H).fill(0), cols = new Array(W).fill(0);
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) { const [r, g, b] = at(im, x, y); if (olive(r, g, b)) { rows[y]++; cols[x]++; } }
  const ry = rows.map((c, y) => (c > W * 0.85 ? y : -1)).filter((y) => y >= 0);
  const cx = cols.map((c, x) => (c > H * 0.35 ? x : -1)).filter((x) => x >= 0);
  if (ry.length < 2 || cx.length < 2) return null;
  const k = src.width / W;
  return [cx[0] * k, ry[0] * k, (cx[cx.length - 1] + 1) * k, (ry[ry.length - 1] + 1) * k];
}
function readUP(src, fr) {
  const [fx0, fy0, fx1, fy1] = UP.f;
  /* the page as if rendered at 150 dpi: the frame put where the reference page has it */
  const sx = (fr[2] - fr[0]) / (fx1 - fx0), sy = (fr[3] - fr[1]) / (fy1 - fy0);
  const W = 1650, H = 1275;
  const im = pixels(src, fr[0] - fx0 * sx, fr[1] - fy0 * sy, W * sx, H * sy, W, H);
  const v = {}, how = {}, marks = [];
  const L = (r, g, b) => (r + g + b) / 3;
  for (const [n, cx, cy] of UP.knobs) {
    /* the pointer: the white stroke from the cap's middle out */
    const b = blob(im, cx - 36, cy - 36, cx + 37, cy + 37, (r, g, bb) => L(r, g, bb) > 238);
    const pts = b.pts.filter(([x, y]) => { const d = Math.hypot(x - cx, y - cy); return d > 5 && d < 34; });
    if (pts.length < 15) { marks.push({ x: cx, y: cy, n, how: "unset" }); continue; }
    let sx2 = 0, sy2 = 0, sw = 0;
    for (const [x, y] of pts) { const w = Math.hypot(x - cx, y - cy); sx2 += (x - cx) * w; sy2 += (y - cy) * w; sw += w; }
    const a = deg(sx2 / sw, sy2 / sw);
    if (POS[n]) { v[P[n]] = nearest(a, POS[n]); how[P[n]] = "ok"; }
    else if (n === "BEND") { v[P.BEND] = Math.round(clamp01((a + 135) / 270) * 11); how[P.BEND] = "ok"; }
    else { v[P[n]] = knobV(a); how[P[n]] = "ok"; }
    marks.push({ x: cx, y: cy, n, how: how[P[n]] });
  }
  for (const [n, cx] of UP.faders) {
    /* the cap: the rows where most of the column is its grey */
    const rows = [];
    for (let y = UP.fTop - 30; y <= UP.fBot + 30; y++) {
      let c = 0;
      for (let x = cx - 20; x <= cx + 20; x++) { const [r, g, b] = at(im, x, y), l = L(r, g, b); if (Math.abs(r - g) < 14 && Math.abs(g - b) < 14 && l > 120 && l < 232) c++; }
      if (c > 26) rows.push(y);
    }
    if (rows.length < 12) { marks.push({ x: cx, y: (UP.fTop + UP.fBot) / 2, n, how: "unset" }); continue; }
    const yc = (rows[0] + rows[rows.length - 1]) / 2;
    v[P[n]] = Math.round(clamp01((UP.fBot - yc) / (UP.fBot - UP.fTop)) * 127); how[P[n]] = "ok";
    marks.push({ x: cx, y: yc, n, how: "ok" });
  }
  for (const [n, cx, y0, y1] of UP.switches) {
    /* the lever: the dark ring on the slot's grey */
    const s = SWITCH[n];
    const slot = blob(im, cx - 6, y0, cx + 7, y1, (r, g, b) => L(r, g, b) < 150);
    const lev = blob(im, cx - 16, y0, cx + 17, y1, (r, g, b) => L(r, g, b) < 60);
    if (slot.n < 40 || lev.n < 15) { marks.push({ x: cx, y: (y0 + y1) / 2, n, how: "unset" }); continue; }
    const ys = slot.pts.map((p) => p[1]), top = Math.min(...ys) + 14, bot = Math.max(...ys) - 14;
    const i = Math.round(clamp01((lev.y - top) / Math.max(1, bot - top)) * (s.n - 1));
    for (const [p, x] of Object.entries(s.to(i))) { v[p] = x; how[p] = s.guess ? "guess" : "ok"; }
    marks.push({ x: cx, y: lev.y, n, how: s.guess ? "guess" : "ok" });
  }
  return { layout: "Ultimate Patches sheet", canvas: im.canvas, v, how, marks, crop: [fx0 - 20, fy0 - 20, fx1 + 20, fy1 + 20] };
}

/* ------------------------------------------------------------ the whole: a file -> its patches */
let pdfjs = null;
async function pdfPages(buf) {
  pdfjs = pdfjs || await import("./pdfjs/pdf.min.mjs");
  pdfjs.GlobalWorkerOptions.workerSrc = new URL("./pdfjs/pdf.worker.min.mjs", import.meta.url).href;
  const doc = await pdfjs.getDocument({ data: buf }).promise, out = [];
  for (let i = 1; i <= doc.numPages; i++) {
    const page = await doc.getPage(i), vp0 = page.getViewport({ scale: 1 });
    const vp = page.getViewport({ scale: 1650 / vp0.width });
    const [c, x] = canvasOf(null, Math.round(vp.width), Math.round(vp.height));
    x.fillStyle = "#fff"; x.fillRect(0, 0, c.width, c.height);
    await page.render({ canvasContext: x, viewport: vp }).promise;
    let title = "";
    try {   /* the largest words in the page's top quarter: its title */
      const tc = await page.getTextContent();
      const items = tc.items.filter((t) => t.str.trim() && t.transform[5] > vp0.height * 0.7);
      const big = Math.max(0, ...items.map((t) => Math.abs(t.transform[3])));
      title = items.filter((t) => Math.abs(t.transform[3]) >= big * 0.95).map((t) => t.str).join(" ").trim();
    } catch (e) { /* (no text) */ }
    out.push({ src: c, title, page: i });
  }
  return out;
}
const STOP = new Set(["THE", "A", "AN", "OF", "AND", "FOR"]);
export const nameFrom = (s) => (s || "").toUpperCase().replace(/\.[A-Z0-9]+$/, "").replace(/[^A-Z0-9]+/g, " ").trim()
  .split(" ").filter((w) => w && !STOP.has(w)).reduce((a, w) => (a.length + (a ? 1 : 0) + w.length <= 8 ? (a ? a + " " : "") + w : a || w.slice(0, 8)), "").slice(0, 8);

/* a file -> [{ name, v: {param: value}, how: {param: "ok"|"guess"}, exact, layout, canvas, marks, page }] (no sheet: []) */
export async function readSheetFile(file) {
  const buf = new Uint8Array(await file.arrayBuffer());
  const lower = file.name.toLowerCase();
  if (lower.endsWith(".svg") || file.type === "image/svg+xml") {
    const t = new TextDecoder().decode(buf), m = t.match(/<metadata id="mb1-patch">([0-9a-f]+)<\/metadata>/);
    const p = m && framePatch(UNHEX(m[1]));
    return p ? [{ ...p, exact: true, layout: "MB-1 patch sheet", how: {} }] : [];
  }
  const own = pngPatch(buf);
  if (own) { const p = framePatch(own); if (p) return [{ ...p, exact: true, layout: "MB-1 patch sheet", how: {} }]; }
  let pages;
  if (lower.endsWith(".pdf") || file.type === "application/pdf") pages = await pdfPages(buf.buffer.slice(0));
  else pages = [{ src: await imageOf(new Blob([buf], { type: file.type || "image/*" })), title: "", page: 1 }];
  const out = [];
  for (const pg of pages) {
    const fr = frameUP(pg.src);
    let r = null;
    if (fr) r = readUP(pg.src, fr);
    else if (isEH(pg.src)) r = readEH(pg.src);
    if (!r || Object.keys(r.v).length < 8) continue;   /* (a page that is no sheet: a cover, a list) */
    out.push({ ...r, name: nameFrom(pg.title || file.name), page: pg.page, exact: false });
  }
  return out;
}
