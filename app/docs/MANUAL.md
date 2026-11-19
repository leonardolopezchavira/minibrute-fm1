# MB-1 manual

MB-1 turns the FM-1 into a monophonic Minibrute-style synthesizer. One voice, last-note priority: hold a key, play
another, let it go, and the first comes back (with glide, if GLIDE is up).

![Playing](img/playing.png)

## The screen

At the top: the patch slot and its name (a `*` when it has changed since it was saved) and the note playing. In the
middle: the page, and its four parameters, one for each of K1..K4, with their values and bars. At the bottom: the mod
wheel, the keys' pressure (aftertouch), the octave, and the sound as a scope.

## The controls

| Control | What it does |
| --- | --- |
| K1..K4 | The four parameters on screen. A fast turn moves in bigger steps. |
| HOME, SEL, ENV, LFO, EDIT, GLO, FX | A page; pressed again, its second page (the button of the page on screen is lit) |
| SELECT | Steps through every page |
| ALGO | The mod wheel (sent as MIDI CC 1 too). Where it goes: PERFORM, WHEEL TO |
| PRESET | The patch slot, 1..16. Turning it loads the slot (unsaved changes are dropped) |
| SAVE | Stores the patch in its slot, and the settings (lit when there is something to save) |
| OCT- / OCT+ | The keyboard's octave, -2..+2 |
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
| TOUCH | FX | AT VIB | AT CUT | PW ENV | MTL ENV |

Notes on a few of them:

- **ULTRASAW** (OSC 2) mixes a phase-shifted copy into the sawtooth; **US RATE** (MORE) is its speed.
- **METAL** (OSC 2) folds the triangle; **MTL ENV** and **PWM/MTL** move it with the filter envelope and the LFO.
- **BRUTE** (FILTER) feeds the filter's output back into its input: growl, then chaos.
- **WHEEL TO** (PERFORM) is the Minibrute's mod wheel destination: LFO AMT, VIBRATO or CUTOFF; **WHL AMT** how far.
- **VINTAGE** (PERFORM) detunes each note a little, the way an old oscillator drifts.

## The patches

Slots 1..8 start with factory sounds: INIT, BASS, ACID, METALEAD, ULTRAPAD, PERC, BRASS, WOBBLE. Slots 9..16 start as
INIT. SAVE writes over the slot shown.

## MIDI

MB-1 plays from USB, Bluetooth and TRS MIDI on any channel: notes, pitch bend (±2 semitones), the mod wheel (CC 1),
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
| 97 | AT VIB | TOUCH |
| 98 | AT CUT | TOUCH |
| 99 | PW ENV | TOUCH |
| 100 | MTL ENV | TOUCH |

Choices (MODE, WAVE, ...) take the CC range in equal parts.
