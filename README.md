# MB-1 for the M-VAVE FM-1

An unofficial, aggressive monophonic virtual analog synthesizer firmware for the M-VAVE FM-1, after the Arturia
MiniBrute: written from scratch and measured against Arturia's MiniBrute V plugin. Built on [fm1-base](https://github.com/charlesvestal/fm1-base), laid
out like [sq80-fm1](https://github.com/leonardolopezchavira/sq80-fm1).

- **Install, patch editor, manual, emulator, cheat sheet:** https://leonardolopezchavira.github.io/minibrute-fm1/
- **Also on [The Firmware Hub](https://thefirmwarehub.com/)**, as MB-1 (identity `FM-1_49XXYYZZ`: 0.15.0 is
  `FM-1_49001500`); its card's text is [`fmhub.json`](fmhub.json).
- **The voice:** sawtooth with Ultrasaw, square with PWM, triangle with Metalizer, sub (square or sine, -1 or -2
  octaves) and noise; a resonant 12 dB LP / BP / HP / notch filter with Brute Factor, key tracking and self-oscillation;
  filter and amp ADSRs; an LFO (six waves) to pitch, filter, PWM/Metalizer and amp; vibrato; the mod wheel to LFO
  amount, vibrato or cutoff; aftertouch; glide; vintage drift.
- **Patches:** 128 slots in four banks of 32 (A01..D32), every parameter saved with the patch and on its own MIDI CC;
  a factory bank of 32 in bank A (`app/factory/MB-1_bank.syx`); RANDOM (HOME three times) for a new one.
- **Performance:** an arpeggiator after the Minibrute's (saved with each patch), and a 64-step sequencer after the
  KeyStep's with 8 patterns (step entry and recording in time; notes, velocity, rests, ties), on one clock: tempo,
  tap tempo or MIDI clock.
- **Effects:** distortion, a Juno-6 chorus, a stereo delay (ping-pong, tempo sync), a plate reverb and a compressor,
  saved with each patch and measured against the plugin's: distortion, chorus and compressor in the voice (no added
  latency), the delay and reverb on the FM-1's second core.
- **The patch editor:** a MiniBrute-style patch sheet in the browser that plays (MB-1's own engine in WebAssembly)
  and edits the FM-1 live over USB; .syx files, and the sheet out as SVG, PNG or print.
- **The screen:** each parameter drawn as on the patch sheet (knob, fader or slide switch), in the site's dark
  colours, after a start-up screen of its own.
- **The panel:** see [the manual](app/docs/MANUAL.md).

| Path | What |
| --- | --- |
| `app/mbvoice/` | The engine: portable C99, float only (`MB_FASTMATH` uses fm1-base's fastmath.h, no libm) |
| `app/app.c` | The FM-1 app: pages, patches, MIDI, the screen |
| `app/mbpatch.h` | A patch's parameters and INIT, and their mapping to the engine (shared with the editor) |
| `app/factory/` | The factory bank as the editor saves it (.syx); `tools/make_factory.py` makes `app/factory_bank.h` from it |
| `app/editor/` | The patch editor: the page, and its engine's wrapper for WebAssembly (`tools/build_editor.sh`) |
| `app/site/` | The front page, its pictures and the font (Barlow Condensed, SIL OFL 1.1); `tools/site_theme.py` styles every page |
| `app/splash/` | The start-up screen (SVG and its 240 x 240 PNG); `tools/make_splash.py` makes `app/splash_img.h` from it |
| `tools/` | The site's and editor's build steps (`build_editor.sh`, `make_home.py`, `site_theme.py`, `make_cheat.py`: the cheat sheet, from the manual's tables), `make_splash.py` and `make_factory.py` |
| `fmhub.json`, `docs/img/card.png` | The Firmware Hub's card: its text and picture |
| `app/tests/basic.txt` | Host simulator checks |
| `fm1/base/` | fm1-base, vendored |

## Build

    python3 fm1/base/tools/build.py app --host        # the host simulator, then:
    app/build/host/mb1_host app/tests/basic.txt app/build/host
    python3 fm1/base/tools/build.py app --release 0.1.0   # the firmware (JieLi toolchain; CI does this)
    sh tools/build_editor.sh /tmp/edit                # the patch editor and its engine (clang, wasm-ld)

Every push to `main` builds the firmware on GitHub Actions (zvenson/fm1-build) and publishes the site: fm1-base's
installer, manual and emulator, then the front page (`tools/make_home.py`), the patch editor
(`tools/build_editor.sh`), the look over all of them (`tools/site_theme.py`) and the cheat sheet
(`tools/make_cheat.py`, its PDF printed by Chrome).

**A release:** set `app/VERSION`, push, then publish a GitHub release with a new tag `vX.Y.Z` on `main` (on the
website, or `git tag vX.Y.Z && git push origin vX.Y.Z`). The tag's run builds and signs the `.fwsc` and attaches it to
the release; The Firmware Hub lists it within three hours. Attach nothing by hand: the hub checks the signature.

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
