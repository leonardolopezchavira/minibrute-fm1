# MB-1 manual

MB-1 turns the FM-1 into an aggressive monophonic virtual analog synthesizer, after the Arturia MiniBrute and
measured against its MiniBrute V plugin. One voice, last-note priority: hold a key, play another, let it go, and the
first comes back (with glide, if GLIDE is up). It keeps 128 patches, has an arpeggiator, a 64-step sequencer and five
effects, and comes with a patch editor in the browser.

![Playing](img/playing.png)

## The screen

MB-1 starts with its own screen: the four waveforms, its name and what it is. Then, in the site's dark colours
(black, the panel's grey, white labels, orange values):

At the top: the patch slot (A01..D32) and its name (a `*` when it has changed since it was saved) and the note playing. In the
middle: the page, and its four parameters, one for each of K1..K4, each drawn as on the patch editor's sheet (a knob, a fader for the mixer and the envelopes, a slide switch for the
two- and three-way choices) with its value; the one being turned lights up, its pointer orange. At the bottom: the mod
wheel, the keys' pressure (aftertouch), the octave, the audio load (CPU: now, PEAK: the most since power-on, in % of the time the FM-1 has for each block of sound; XRUN: blocks it missed, heard as clicks), and the sound as a scope.

## The controls

| Control | What it does |
| --- | --- |
| K1..K4 | The four parameters on screen. A fast turn moves in bigger steps. |
| HOME, SEL, ENV, LFO, EDIT, GLO, FX | A page; pressed again, its next page (FX: TOUCH, DELAY, REVERB, FX 2, DIST, CHORUS, COMP; the button of the page on screen is lit) |
| SELECT | Steps through every page |
| ALGO | The mod wheel (sent as MIDI CC 1 too). Where it goes: PERFORM, WHEEL TO |
| PRESET | The patch slot, A01..D32: 128 slots in four banks of 32. Turning it loads the slot (unsaved changes are dropped) |
| SAVE | Stores the patch in its slot, and the settings (lit when there is something to save) |
| OCT- / OCT+ | The keyboard's octave, -2..+2 |
| ARP | The arpeggiator: OFF, ON, HOLD in turn (and its page). Held half a second: its pages, ARP and ARP 2. Lit while it runs, dark a moment on each beat |
| SEQ | The sequencer's page |
| PLAY | The sequencer: start (from the first step) and stop. Lit while it plays, dark a moment on each beat. With SEQ held: tap tempo, tap it on the beat |
| REC | Arms recording into the sequencer: stopped, step by step; playing, in time. Lit while armed |
| The keys | Notes with velocity; pressing harder is aftertouch |

## The pages

| Page | Button | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- | --- |
| FILTER | HOME | CUTOFF | RESO | BRUTE | ENV AMT |
| FILTER 2 | HOME | MODE | KBD TRK | ENV VEL | AMP VEL |
| OSC | SEL | SAW | SQUARE | TRI | SUB |
| OSC 2 | SEL | NOISE | ULTRASAW | PW | METAL |
| FILT ENV | ENV | ATTACK | DECAY | SUSTAIN | RELEASE |
| AMP ENV | ENV | ATTACK | DECAY | SUSTAIN | RELEASE |
| LFO | LFO | RATE | WAVE | PITCH | FILTER |
| LFO 2 | LFO | PWM/MTL | AMP | VIB RATE | VIB TYPE |
| PERFORM | EDIT | GLIDE | VINTAGE | WHEEL TO | WHL AMT |
| MORE | GLO | SUB OCT | SUB WAVE | US RATE | LEVEL |
| VOICE | GLO | PLAY | BEND | LFO RTRG |  |
| TOUCH | FX | AT VIB | AT CUT | PW ENV | MTL ENV |
| DELAY | FX | TIME | FEEDBACK | TONE | MIX |
| REVERB | FX | DECAY | SIZE | DAMP | MIX |
| FX 2 | FX | DLY SYNC | DLY MODE | PRE-DLY | FX ORDER |
| DIST | FX | TYPE | DRIVE | TONE | MIX |
| CHORUS | FX | RATE | DEPTH | STEREO | MIX |
| COMP | FX | THRESH | RATIO | ATTACK | RELEASE |
| ARP | ARP | MODE | OCTAVE | STEP | SWING |
| ARP 2 | ARP (held) | TEMPO | ARP | HOLD ORD | LFO CLK |
| SEQ | SEQ | PATTERN | LENGTH | STEP | SWING |

Notes on a few of them:

- **ULTRASAW** (OSC 2) adds two shifting copies of the sawtooth, as the hardware does: one moved by a slow LFO of its
  own, one by **US RATE** (MORE).
- **METAL** (OSC 2) folds the triangle through four folding stages (low settings fold only its top); **MTL ENV** and
  **PWM/MTL** move it with the filter envelope and the LFO.
- **BRUTE** (FILTER) feeds the sound after the VCA back into the oscillator mixer, as the hardware does: growl, then,
  near full, chaos (a low growl a third of the note's pitch). It follows the amp envelope.
- **MODE** (FILTER 2): LP, BP, HP and NOTCH as the Minibrute's Steiner-Parker filter makes them: HP keeps some low
  end, the notch is shallow.
- **WHEEL TO** (PERFORM) is the Minibrute's mod wheel destination: LFO AMT, VIBRATO or CUTOFF; **WHL AMT** how far.
- **VINTAGE** (PERFORM) detunes each note a little, the way an old oscillator drifts.
- **PLAY** (VOICE): LEGATO, a note played while another is held keeps the envelopes going (and glides); RETRIG, every
  note restarts them. **BEND** is the pitch bend's range, 1..12 semitones. **LFO RTRG** restarts the LFO with each
  note that starts the envelopes.

## The arpeggiator

After the Minibrute's. ARP steps through OFF, ON and HOLD:

- **ON** plays the keys while you hold them.
- **HOLD** keeps playing the last chord when you let go; a key pressed with none held starts a new one. With
  **HOLD ORD** PLAYED (ARP 2), the notes play in the order you pressed them, up to 16, a note as often as you
  play it: hold the first key down and tap the others (C held, then E, D, E: C E D E); with PITCH, a chord, low to
  high.
- **MODE**: UP, DOWN, UP/DOWN (the top and bottom notes play twice, as on the Minibrute) or RANDOM.
- **OCTAVE**: over 1 to 4 octaves.
- **STEP**: 1/4, 1/8, 1/16, or their triplets 1/4T, 1/8T, 1/16T.
- **SWING**: 50 % (straight) to 75 % in six steps: every second note comes later.
- **TEMPO** (ARP 2): 30 to 260 BPM; or hold SEQ and tap PLAY on the beat. When MIDI clock comes in (USB, Bluetooth or TRS),
  the arpeggiator follows it, and TEMPO at its lowest halves it, at its highest doubles it.
- **LFO CLK** (ARP 2): ARP puts the LFO on the arpeggiator's tempo; LFO RATE then picks a note value (8 bars
  dotted .. a 64th triplet), as the plugin's synced LFO.

Each note sounds for half its step. As in the plugin, the arpeggiator is the patch's: ARP (OFF, ON or HOLD), MODE,
OCTAVE, STEP and SWING are saved with it, so picking a patch can switch the arpeggiator on. TEMPO, HOLD ORD and
LFO CLK belong to the instrument (one tempo for every patch).

## The effects

Five effects, after the plugin's and measured against them, saved with each patch: distortion, chorus, delay, reverb
and a compressor, in that order (FX ORDER can put the reverb before the delay).

**Delay and reverb.** **FX ORDER** (FX 2): DLY>REV (the repeats into the
reverb, the usual) or REV>DLY (the reverb's tail repeated), as each of the plugin's presets has them. They are stereo: the
voice in the middle, the repeats and the reverb around it. With MIX at 0 an effect is off; half way, dry and wet are
both at -3 dB (an equal-power crossfade, as the plugin's). Matched to the plugin's delay and reverb by measurement.

- **DELAY**: **TIME** 2 ms to 1.5 s (0.74 s in PING-PONG), on the plugin's scale; with **DLY SYNC** CLOCK (FX 2) it
  picks a note value instead (1/32 .. 1/1, triplets and dotted too) on the arpeggiator's and sequencer's tempo.
  **FEEDBACK** the repeats; **TONE** a low-pass on them (12 dB an octave; each repeat darker); **MIX** dry to wet. **DLY MODE** (FX 2):
  STEREO (the repeats in the middle) or PING-PONG (left, right, left...). Turning TIME glides, as tape does.
- **REVERB**: a plate. **DECAY** how long it rings (about 0.7 to 9 s at SIZE half way, as the plugin's), **SIZE** the
  room (it scales the decay too), **DAMP** how quickly it dies away, darker as it goes; **MIX** dry to wet; **PRE-DLY**
  (FX 2) a gap before it, 0 to 200 ms.

They run on the FM-1's second core while the first plays the voice (the screen's foot shows **2C**). On a unit whose
second core is unreliable (some are), MB-1 notices, gives it up until power-off, and runs them on the first (**1C**):
the same sound, with less headroom. Only the effects are a block (6 ms) behind the voice; the dry sound is not.

**Distortion, chorus, compressor**, in the sound itself: the distortion first, then the chorus, then the delay and
reverb (FX ORDER), the compressor last.

- **DIST**: **TYPE** SOFT (a rounded clip: the plugin's types 12, 13), HARD (10), ASYM (lopsided, even harmonics: 6, 7)
  or FOLD (folds back on itself as DRIVE rises: 9); **DRIVE** 0 to 48 dB (the level made up as it rises); **TONE** a
  low-pass after it, 1 to 20 kHz; **MIX**.
- **CHORUS**: the Juno-6's: **RATE** 0.33 to 15 Hz, **DEPTH** its sweep (up to 10 ms), **STEREO** the right's sweep
  against the left's (180 degrees: opposite, the Juno's), **MIX**.
- **COMP**: **THRESH** -60 to +20 dB, **RATIO** OFF (1:1) to 100:1, **ATTACK**, **RELEASE**; its makeup gain is
  automatic. On when RATIO is above 1:1.

These three run on the first core, in the sound itself, so they add no delay: the dry sound is never late. The
distortion and chorus come before the delay and reverb (which hear them); the compressor comes last, on everything.

## The sequencer

A 64-step monophonic step sequencer, after the KeyStep's (as on the MiniBrute 2), with 8 patterns. Each step is a
note with its velocity, a rest, or a tie (the note before goes on through it). It runs on the arpeggiator's clock:
TEMPO, a tap (SEQ held + PLAY), or MIDI clock (USB, Bluetooth or TRS) while it comes; the LFO follows it too with
LFO CLK on ARP.

- **The SEQ page**: **PATTERN** 1..8, **LENGTH** 1..64 steps, **STEP** (1/4 .. 1/16T) and **SWING** (50..75 %), its own
  (not the arpeggiator's). A pattern changed while it plays takes over at the next step.
- **PLAY** starts it from the first step and stops it. While it plays, the arpeggiator rests and the keys play over
  it. MIDI Start, Continue and Stop run it too when the patch's arpeggiator is off.
- **Step entry**: stopped, press **REC** (lit; the screen shows REC and the step). Each key writes a step and moves on;
  **FX** writes a rest; holding the key and pressing **FX** writes a tie (the note goes on through that step). The
  pattern is as long as the steps entered (LENGTH changes it later). Press REC again to finish.
- **Recording in time**: while it plays, press **REC**. A key goes to the nearest step (the one playing, or the next
  if it is more than half over); held over the steps after, it writes ties. Press REC again to finish.

Each note sounds for half its step, or on through its ties. The patterns are stored by themselves (a couple of
seconds after a change, while stopped; SAVE also stores them) and kept over power-off.

## The patches

128 slots in four banks of 32, A01..D32: turn PRESET to pick one. Slots A01..A08 start with factory sounds: INIT, BASS, ACID, METALEAD, ULTRAPAD, PERC, BRASS, WOBBLE; the rest
start as INIT. SAVE writes over the slot shown. (Before 0.12 there were 32 slots: they are bank A now, as they were.) The
patch editor (below) reads, edits and stores all of them, and keeps banks as .syx files.

## The knobs' range

Every knob runs over the same range, with the same feel, as the MiniBrute V plugin's: the cutoff from 20 Hz to 20 kHz,
the attacks to 12.5 s, the decays and releases to 25 s, KBD TRK to 200 %, GLIDE to 2 s an octave, the LFO from
0.05 to 100 Hz, VIB RATE from 1 to 50 Hz, US RATE from 0.005 to 43 Hz. A knob at the same place as the plugin's
knob is the same setting.

## The patch editor

The site's [patch editor](../edit/) is a MiniBrute-style patch sheet that plays: every value of a patch on one
panel, drawn like the paper sheets MiniBrute players fill in and share, in light or dark. It sounds in the browser with MB-1's own engine (the arpeggiator too, at the editor's
tempo) and, with the FM-1 connected by USB (Chrome or Edge), on the FM-1 as you turn each control. It reads the
FM-1's 128 slots (banks A..D) and the sound playing, stores a patch in a slot, writes a whole bank, opens and saves
.syx files, and exports the sheet as SVG or PNG, or prints it. Reading the FM-1 needs MB-1 0.11.4 or later, all 128
slots 0.12 or later. Every control on the sheet has its CC (below): a controller or a DAW can play them the same way.

## Loading patches from a computer

MB-1 takes patches over USB MIDI SysEx (`F0 7D 4D 42 ...`, app/app.c "the loader"): a patch into a slot (01; slots
0..127 are A01..D32), the slots stored (02), bank A's factory patches back (03; banks B..D are left alone), a slot played (04), a slot's patch sent
back (05), what is there (06: the parameter count, the banks of 32, the slot playing, edited or not), a patch played
without a slot (07, as an edit: SAVE keeps it), and the sound playing sent back, edits included (08). Each frame is
answered.

## MIDI

MB-1 plays from USB, Bluetooth and TRS MIDI on any channel: notes, pitch bend (VOICE: BEND, 1..12 semitones), the mod wheel (CC 1),
aftertouch (channel or poly), all notes off (CC 120, 123), and every parameter by its CC. The keys and the knobs are
sent out over USB and Bluetooth.

| CC | Parameter | Page |
| --- | --- | --- |
| 74 | CUTOFF | FILTER |
| 71 | RESO | FILTER |
| 20 | BRUTE | FILTER |
| 21 | ENV AMT | FILTER |
| 22 | MODE | FILTER 2 |
| 23 | KBD TRK | FILTER 2 |
| 24 | ENV VEL | FILTER 2 |
| 25 | AMP VEL | FILTER 2 |
| 26 | SAW | OSC |
| 27 | SQUARE | OSC |
| 28 | TRI | OSC |
| 29 | SUB | OSC |
| 30 | NOISE | OSC 2 |
| 31 | ULTRASAW | OSC 2 |
| 75 | PW | OSC 2 |
| 76 | METAL | OSC 2 |
| 77 | ATTACK | FILT ENV |
| 78 | DECAY | FILT ENV |
| 79 | SUSTAIN | FILT ENV |
| 80 | RELEASE | FILT ENV |
| 73 | ATTACK | AMP ENV |
| 81 | DECAY | AMP ENV |
| 82 | SUSTAIN | AMP ENV |
| 72 | RELEASE | AMP ENV |
| 83 | RATE | LFO |
| 84 | WAVE | LFO |
| 85 | PITCH | LFO |
| 86 | FILTER | LFO |
| 87 | PWM/MTL | LFO 2 |
| 88 | AMP | LFO 2 |
| 89 | VIB RATE | LFO 2 |
| 90 | VIB TYPE | LFO 2 |
| 5 | GLIDE | PERFORM |
| 91 | VINTAGE | PERFORM |
| 92 | WHEEL TO | PERFORM |
| 93 | WHL AMT | PERFORM |
| 94 | SUB OCT | MORE |
| 95 | SUB WAVE | MORE |
| 96 | US RATE | MORE |
| 7 | LEVEL | MORE |
| 102 | PLAY | VOICE |
| 103 | BEND | VOICE |
| 104 | LFO RTRG | VOICE |
| 105 | ARP (off, on, hold) | ARP 2 |
| 106 | MODE | ARP |
| 107 | OCTAVE | ARP |
| 108 | STEP | ARP |
| 109 | SWING | ARP |
| 110 | TIME | DELAY |
| 111 | FEEDBACK | DELAY |
| 112 | TONE | DELAY |
| 113 | MIX | DELAY |
| 114 | DLY SYNC | FX 2 |
| 115 | DLY MODE | FX 2 |
| 116 | DECAY | REVERB |
| 117 | SIZE | REVERB |
| 118 | DAMP | REVERB |
| 119 | MIX | REVERB |
| 14 | PRE-DLY | FX 2 |
| 15 | FX ORDER | FX 2 |
| 16 | TYPE | DIST |
| 17 | DRIVE | DIST |
| 18 | TONE | DIST |
| 19 | MIX | DIST |
| 52 | RATE | CHORUS |
| 53 | DEPTH | CHORUS |
| 54 | STEREO | CHORUS |
| 55 | MIX | CHORUS |
| 56 | THRESH | COMP |
| 57 | RATIO | COMP |
| 58 | ATTACK | COMP |
| 59 | RELEASE | COMP |
| 97 | AT VIB | TOUCH |
| 98 | AT CUT | TOUCH |
| 99 | PW ENV | TOUCH |
| 100 | MTL ENV | TOUCH |

Choices (MODE, WAVE, ...) take the CC range in equal parts.
