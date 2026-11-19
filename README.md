# MB-1 for the M-VAVE FM-1

An unofficial Minibrute-style monophonic synthesizer firmware for the M-VAVE FM-1, written from scratch and
measured against Arturia's MiniBrute V plugin. Built on [fm1-base](https://github.com/charlesvestal/fm1-base), laid
out like [sq80-fm1](https://github.com/leonardolopezchavira/sq80-fm1).

- **Install, manual, emulator:** https://leonardolopezchavira.github.io/minibrute-fm1/ (after the first site build)
- **The voice:** sawtooth with Ultrasaw, square with PWM, triangle with Metalizer, sub (square or sine, -1 or -2
  octaves) and noise; a resonant 12 dB LP / BP / HP / notch filter with Brute Factor, key tracking and self-oscillation;
  filter and amp ADSRs; an LFO (six waves) to pitch, filter, PWM/Metalizer and amp; vibrato; the mod wheel to LFO
  amount, vibrato or cutoff; aftertouch; glide; vintage drift.
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

The engine's calibration (`app/mbvoice/mbcal.h`) was fitted to renders of the MiniBrute V plugin. The Metalizer in
this repository is a parametric folder of its own; the measured single cycles used during development are not
published (`MB_MEASURED_METAL`, private builds only).

## Licence

MIT (LICENSE). fm1-base is MPL-2.0 (fm1/base/LICENSE). Arturia, Minibrute and MiniBrute V are trademarks of Arturia;
M-VAVE and FM-1 of their owner. This project is not affiliated with either.
