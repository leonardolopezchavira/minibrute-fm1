# MB-1 for the M-VAVE FM-1

An unofficial Minibrute-style monophonic synthesizer firmware for the M-VAVE FM-1, written from scratch and
measured against Arturia's MiniBrute V plugin. Built on [fm1-base](https://github.com/charlesvestal/fm1-base), laid
out like [sq80-fm1](https://github.com/leonardolopezchavira/sq80-fm1).

- **Install, manual, emulator:** https://leonardolopezchavira.github.io/minibrute-fm1/ (after the first site build)
- **The voice:** sawtooth with Ultrasaw, square with PWM, triangle with Metalizer, sub (square or sine, -1 or -2
  octaves) and noise; a resonant 12 dB LP / BP / HP / notch filter with Brute Factor, key tracking and self-oscillation;
  filter and amp ADSRs; an LFO (six waves) to pitch, filter, PWM/Metalizer and amp; vibrato; the mod wheel to LFO
  amount, vibrato or cutoff; aftertouch; glide; vintage drift.
- **Performance:** an arpeggiator after the Minibrute's (saved with each patch), and a 64-step sequencer after the
  KeyStep's with 8 patterns (step entry and recording in time; notes, velocity, rests, ties), on one clock: tempo,
  tap tempo or MIDI clock.
- **Effects:** distortion, a Juno-6 chorus, a stereo delay (ping-pong, tempo sync), a plate reverb and a compressor,
  saved with each patch and measured against the plugin's, on the FM-1's second core.
- **The panel:** see [the manual](app/docs/MANUAL.md).

| Path | What |
| --- | --- |
| `app/mbvoice/` | The engine: portable C99, float only (`MB_FASTMATH` uses fm1-base's fastmath.h, no libm) |
| `app/app.c` | The FM-1 app: pages, patches, MIDI, the screen |
| `app/tests/basic.txt` | Host simulator checks |
| `fm1/base/` | fm1-base, vendored |

## Build

    python3 fm1/base/tools/build.py app --host        # the host simulator, then:
    app/build/host/mb1_host app/tests/basic.txt app/build/host
    python3 fm1/base/tools/build.py app --release 0.1.0   # the firmware (JieLi toolchain; CI does this)

Every push to `main` builds the firmware on GitHub Actions (zvenson/fm1-build) and publishes the site.

## About the measurements

The engine's calibration (`app/mbvoice/mbcal.h`) was fitted to renders of the MiniBrute V plugin, and its
structure follows the Minibrute's circuit where that made it closer:

- **Brute Factor** takes the VCA's output back into the oscillator mixer, through the VCA's saturation and a coupling
  capacitor.
- **Ultrasaw** adds two copies of the saw, each with a pulse where the saw crosses a slow triangle LFO (one fixed, one
  the RATE knob).
- **Metalizer**: one folding stage computed from its transistors and resistors (`app/mbvoice/metal_stage.h`), four in
  a row, the folded path a little behind the clean triangle; gain, offset and lag fitted to the plugin.
- **Filter modes**: BP, HP and Notch as mixes of the low-, band- and high-pass responses, as the Steiner-Parker
  network gives them when the signal goes into its different inputs.

The measured Metalizer cycles used during development are not published (`MB_MEASURED_METAL`, private builds only).

## Licence

MIT (LICENSE). fm1-base is MPL-2.0 (fm1/base/LICENSE). Arturia, Minibrute and MiniBrute V are trademarks of Arturia;
M-VAVE and FM-1 of their owner. This project is not affiliated with either.
