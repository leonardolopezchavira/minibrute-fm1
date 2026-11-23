#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""MB-1's cheat sheet: two letter pages (landscape) to keep beside the FM-1, in the look of a MiniBrute patch sheet.
The pages and the CCs come from the manual's tables (app/docs/MANUAL.md), the factory names from app/factory_bank.h,
so it follows them. Writes OUT_DIR/index.html, and OUT_DIR/MB-1_cheatsheet.pdf when Chrome is there to print it.

    python3 tools/make_cheat.py OUT_DIR VERSION
"""
import html
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
out, version = Path(sys.argv[1]), sys.argv[2]
out.mkdir(parents=True, exist_ok=True)
manual = (ROOT / "app" / "docs" / "MANUAL.md").read_text(encoding="utf-8")
SITE = "leonardolopezchavira.github.io/minibrute-fm1"


def table(after):
    """the rows of the first markdown table after the heading AFTER"""
    s = manual[manual.index(after):]
    rows = []
    for line in s.splitlines()[1:]:
        if line.startswith("|"):
            cells = [c.strip() for c in line.strip().strip("|").split("|")]
            if not set("".join(cells)) <= set("- "):
                rows.append(cells)
        elif rows:
            break
    return rows[1:]


pages = table("## The pages")             # Page, Button, K1..K4
ccs = table("## MIDI")                    # CC, Parameter, Page
names = re.findall(r'\{"([^"]*)"', (ROOT / "app" / "factory_bank.h").read_text())
e = html.escape

# the pages, by button, in the panel's order
order = []
for p in pages:
    b = p[1].replace(" (held)", "")
    if b not in order:
        order.append(b)
pg_rows = []
for b in order:
    mine = [p for p in pages if p[1].replace(" (held)", "") == b]
    for i, p in enumerate(mine):
        btn = f'<th rowspan="{len(mine)}" class="btn"><span>{e(b)}</span></th>' if i == 0 else ""
        held = ' <i>held</i>' if "(held)" in p[1] else ""
        ks = "".join(f"<td>{e(k)}</td>" for k in (p[2:6] + [""] * 4)[:4])
        pg_rows.append(f'<tr class="{"first" if i == 0 else ""}">{btn}<td class="pg">{e(p[0])}{held}</td>{ks}</tr>')

# the CCs, grouped by page, in the pages' order
by_page = {}
for cc, par, page in ccs:
    by_page.setdefault(page, []).append((int(cc), par.split(" (")[0]))
cc_rows = []
for p in pages:
    if p[0] in by_page:
        items = " ".join(f'<span class="cc"><b>{c}</b>&#8239;{e(n)}</span>' for c, n in by_page.pop(p[0]))
        cc_rows.append(f'<tr><td class="pg">{e(p[0])}</td><td>{items}</td></tr>')

bank = "".join(f'<li><b>A{i + 1:02d}</b> {e(n)}</li>' for i, n in enumerate(names))

CONTROLS = [
    ("K1&ndash;K4", "the four parameters on screen; turn fast for big steps"),
    ("Section buttons",
     "HOME, SEL, ENV, LFO, EDIT, GLO, FX: a section of the patch, its first page; again, its next page; <b>held</b>, back to the first"),
    ("SELECT", "steps through every page"),
    ("PRESET", "the slot, A01&ndash;D32 (4 banks of 32); turning it loads the slot"),
    ("ALGO", "the mod wheel (CC 1); where it goes: EDIT &rsaquo; CONTROLS &rsaquo; WHEEL TO"),
    ("OCT&minus; / OCT+", "the keyboard, &minus;2 to +2 octaves"),
    ("Keys", "velocity; press harder for aftertouch"),
]
SHORTCUTS = [
    ("HOME HOME HOME", "<b>RANDOM</b>: a new random patch (three presses within 0.4&nbsp;s); SAVE keeps it"),
    ("SAVE", "the save screen: <b>K1</b> moves along the name, <b>K2</b> changes the letter, <b>PRESET</b> picks the slot"),
    ("SAVE again", "stores it there; any other button cancels"),
    ("SAVE SAVE", "stores the patch in its own slot"),
    ("ARP", "OFF &rarr; ON &rarr; HOLD; <b>held</b>: its pages ARP and ARP&nbsp;2"),
    ("SEQ", "the sequencer's page"),
    ("PLAY", "the sequencer: start from step 1 / stop"),
    ("SEQ held + PLAY", "tap tempo, on the beat"),
    ("REC (stopped)", "step entry: a key writes a step; <b>FX</b> a rest; key held + <b>FX</b> a tie; REC ends"),
    ("REC (playing)", "records in time, to the nearest step; REC ends"),
]
SCREEN = [
    ("A01 NAME *", "the slot and name; * changed since saved"),
    ("CPU / PEAK", "audio load now / most since power-on"),
    ("XRUN", "missed blocks (clicks)"),
    ("2C / 1C", "effects on the second core / on the first"),
]


def dl(items):
    return "".join(f"<tr><th>{k}</th><td>{v}</td></tr>" for k, v in items)


page = f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>MB-1 cheat sheet</title>
<style>
  @font-face {{ font-family: "Barlow Condensed"; font-weight: 500; src: url("../fonts/barlow-condensed-latin-500-normal.woff2") format("woff2"); }}
  @font-face {{ font-family: "Barlow Condensed"; font-weight: 600; src: url("../fonts/barlow-condensed-latin-600-normal.woff2") format("woff2"); }}
  @font-face {{ font-family: "Barlow Condensed"; font-weight: 700; src: url("../fonts/barlow-condensed-latin-700-normal.woff2") format("woff2"); }}
  @page {{ size: 11in 8.5in; margin: 0; }}
  :root {{ --body: #c4c4c4; --face: #fff; --ink: #000; --mute: #555; --hi: #f06028; --line: #b5b5b5; }}
  * {{ box-sizing: border-box; }}
  html, body {{ margin: 0; background: #e9e9e7; color: var(--ink); font: 500 10.5pt/1.25 "Barlow Condensed", "Arial Narrow", sans-serif;
    -webkit-print-color-adjust: exact; print-color-adjust: exact; }}
  .sheet {{ width: 11in; height: 8.5in; margin: 0 auto 24px; padding: .32in .38in; background: var(--body); overflow: hidden;
    display: grid; grid-template-rows: auto 1fr auto; gap: .14in; page-break-after: always; break-after: page; }}
  @media print {{ html, body {{ background: none; }} .sheet {{ margin: 0; }} }}
  header {{ display: flex; align-items: baseline; gap: 14px; }}
  header h1 {{ margin: 0; font-weight: 700; font-size: 34pt; letter-spacing: .02em; line-height: 1; }}
  header h1 em {{ font-style: normal; color: var(--hi); }}
  header p {{ margin: 0; text-transform: uppercase; letter-spacing: .08em; font-weight: 600; font-size: 11pt; }}
  header .v {{ margin-left: auto; font-weight: 600; font-size: 11pt; color: var(--mute); text-transform: uppercase; letter-spacing: .08em; }}
  .cols {{ display: grid; gap: .16in; min-height: 0; }}
  .p1 {{ grid-template-columns: 1fr 1.15fr; }}
  .p2 {{ grid-template-columns: 1.35fr 1fr; }}
  .stack {{ display: flex; flex-direction: column; gap: .16in; min-height: 0; }}
  section {{ background: var(--face); border-radius: 3px; padding: 13px 11px 7px; position: relative; margin-top: 9px; }}
  section > h2 {{ position: absolute; top: -10px; left: 10px; margin: 0; padding: 2px 10px 1px; background: var(--ink); color: #fff;
    font-weight: 700; font-size: 11pt; letter-spacing: .1em; text-transform: uppercase; border-radius: 2px; }}
  section > h2.hot {{ background: var(--hi); }}
  table {{ border-collapse: collapse; width: 100%; }}
  th, td {{ text-align: left; vertical-align: top; padding: 2px 5px; }}
  .kv th {{ font-weight: 700; text-transform: uppercase; letter-spacing: .03em; width: 30%; padding-right: 10px; }}
  .kv tr + tr > * {{ border-top: 1px solid #e6e6e6; }}
  .pages {{ font-size: 10pt; }}
  .pages thead th {{ font-weight: 700; color: var(--mute); font-size: 8.5pt; letter-spacing: .1em; text-transform: uppercase; padding-bottom: 3px; }}
  .pages td {{ text-transform: uppercase; letter-spacing: .02em; padding: 1.6px 5px; }}
  .pages td.pg {{ font-weight: 700; white-space: nowrap; }}
  .pages td.pg i {{ font-style: normal; font-weight: 500; color: var(--mute); font-size: 8pt; }}
  .pages tr.first > * {{ border-top: 1.5px solid var(--ink); }}
  .pages th.btn {{ width: 1%; padding: 3px 6px 0 0; }}
  .pages th.btn span {{ display: inline-block; min-width: 44px; text-align: center; background: #2b2b2b; color: #fff; border-radius: 3px;
    padding: 2px 6px; font-size: 9pt; letter-spacing: .06em; }}
  .ccs td {{ padding: 2px 5px; font-size: 9.6pt; }}
  .ccs td.pg {{ font-weight: 700; white-space: nowrap; text-transform: uppercase; width: 1%; padding-right: 10px; }}
  .ccs tr + tr > * {{ border-top: 1px solid #e6e6e6; }}
  .cc {{ display: inline-block; margin-right: 10px; text-transform: uppercase; white-space: nowrap; }}
  .cc b {{ color: var(--hi); font-weight: 700; }}
  .bank {{ columns: 4; column-gap: 12px; list-style: none; margin: 0; padding: 0; font-size: 10pt; text-transform: uppercase; }}
  .bank li {{ break-inside: avoid; padding: 1px 0; }}
  .bank b {{ color: var(--hi); font-weight: 700; margin-right: 3px; }}
  .note {{ color: var(--mute); font-size: 9.2pt; margin: 5px 0 0; }}
  footer {{ display: flex; gap: 18px; font-size: 9.5pt; text-transform: uppercase; letter-spacing: .06em; font-weight: 600; color: #333; }}
  footer span b {{ color: var(--ink); }}
  footer .r {{ margin-left: auto; }}
</style>
</head>
<body>

<div class="sheet">
  <header><h1>MB&#8209;<em>1</em></h1><p>Aggressive monophonic virtual analog &middot; M-VAVE FM-1</p><span class="v">Cheat sheet &middot; {e(version)}</span></header>
  <div class="cols p1">
    <div class="stack">
      <section><h2>Controls</h2><table class="kv">{dl(CONTROLS)}</table></section>
      <section><h2 class="hot">Shortcuts</h2><table class="kv">{dl(SHORTCUTS)}</table></section>
      <section><h2>The screen</h2><table class="kv">{dl(SCREEN)}</table></section>
    </div>
    <div class="stack">
    <section><h2>The pages</h2>
      <table class="pages"><thead><tr><th></th><th>Page</th><th>K1</th><th>K2</th><th>K3</th><th>K4</th></tr></thead>
      <tbody>{"".join(pg_rows)}</tbody></table>
      <p class="note">GLO holds the instrument's settings (one tempo for every patch); everything else is saved with the patch.</p>
    </section>
    </div>
  </div>
  <footer><span><b>Online</b> {SITE}/</span><span class="r"><b>edit/</b> patch editor &middot; <b>emu/</b> emulator &middot; <b>manual/</b> &middot; <b>install/</b></span></footer>
</div>

<div class="sheet">
  <header><h1>MB&#8209;<em>1</em></h1><p>MIDI &middot; arpeggiator &middot; sequencer &middot; patches</p><span class="v">Cheat sheet &middot; {e(version)}</span></header>
  <div class="cols p2">
    <section><h2>MIDI CC</h2>
      <table class="ccs">{"".join(cc_rows)}</table>
      <p class="note">USB, Bluetooth and TRS, any channel. Also: notes, pitch bend (PLAY &rsaquo; BEND), CC 1 mod wheel, aftertouch,
      CC 120/123 all notes off, MIDI clock (arpeggiator, sequencer, synced LFO and delay). Choices take the CC range in equal parts.</p>
    </section>
    <div class="stack">
      <section><h2>Arpeggiator</h2><table class="kv">
        <tr><th>ARP</th><td>OFF &rarr; ON (while keys are held) &rarr; HOLD (keeps the last chord)</td></tr>
        <tr><th>Mode</th><td>UP, DOWN, UP/DOWN, RANDOM &middot; 1&ndash;4 octaves &middot; 1/4&ndash;1/16T &middot; swing 50&ndash;75&nbsp;%</td></tr>
        <tr><th>Hold ord</th><td>PLAYED: in the order pressed, up to 16 &middot; PITCH: low to high</td></tr>
        <tr><th>Tempo</th><td>30&ndash;260 BPM, tap (SEQ held + PLAY), or MIDI clock</td></tr>
      </table></section>
      <section><h2>Sequencer</h2><table class="kv">
        <tr><th>Size</th><td>64 steps &times; 8 patterns: note + velocity, rest or tie</td></tr>
        <tr><th>Play</th><td>PLAY starts / stops; the keys play over it; the arpeggiator rests</td></tr>
        <tr><th>Record</th><td>REC stopped: step by step &middot; REC playing: in time</td></tr>
      </table></section>
      <section><h2 class="hot">Factory bank A</h2><ul class="bank">{bank}</ul>
        <p class="note">Banks B&ndash;D start as INIT. The patch editor's <b>Factory bank A</b> puts these back (B&ndash;D stay).</p></section>
    </div>
  </div>
  <footer><span><b>Patch editor</b> reads, edits and stores all 128 slots &middot; keeps banks as .syx &middot; reads patch sheets</span><span class="r"><b>Source</b> github.com/leonardolopezchavira/minibrute-fm1</span></footer>
</div>

</body>
</html>
"""
(out / "index.html").write_text(page, encoding="utf-8")
print(f"cheat    {out / 'index.html'}")

chrome = next((c for c in ("google-chrome", "google-chrome-stable", "chromium", "chromium-browser") if shutil.which(c)), None)
if chrome:
    pdf = out / "MB-1_cheatsheet.pdf"
    subprocess.run([chrome, "--headless=new", "--no-sandbox", "--disable-gpu", "--no-pdf-header-footer",
                    f"--print-to-pdf={pdf}", (out / "index.html").resolve().as_uri()],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=120)
    print(f"cheat    {pdf}")
else:
    print("cheat    no Chrome: no PDF")
