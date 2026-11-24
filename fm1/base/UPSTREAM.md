# SQ-80's own copy of fm1-base

This directory is a copy of [fm1-base](https://github.com/charlesvestal/fm1-base) (MPL-2.0, Charles Vestal and
contributors) at commit `b0976f295ccaeaf77a8053a5a785a9cc21f5345f`, with its `bsp/` submodule, [felucca-bsp](https://github.com/hugelton/felucca-bsp)
(MPL-2.0), at commit `5b0879bfadc69a17dbebb917fb6b86d516f19c44`, kept in SQ-80's repository so that SQ-80 can add the M-Vave SMK-37 as a board.

Their licences and notices are unchanged: `LICENSE`, `LICENSING.md`, `bsp/LICENSE`, `bsp/NOTICE`. Changes made
here for SQ-80 are listed in SQ-80's git history; the SMK-37 board files say where they come from.

MB-1 (minibrute-fm1) adds, for the SMK-37: K1 as the app's knob (`plat_smk_k1_raw`, `plat_smk_k1` in
`include/plat.h`; `bsp/boards/smk37/fm1_adc.h`, `device/plat_fm1.c`, and the simulator's `k1` command in
`host/plat_host.c`). Off, K1 is MASTER as before.
